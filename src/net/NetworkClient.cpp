#include "NetworkClient.h"

#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cstring>
#include <stdexcept>

#include "core/global.h"
#include "config/ConfigVariable.h"
#include "util/NetContext.h"

namespace {
inline boost::system::error_code make_err(boost::asio::error::basic_errors err)
{
    return make_error_code(err);
}

inline boost::system::error_code make_err(boost::system::errc::errc_t err)
{
    return make_error_code(err);
}

inline boost::system::error_code make_err(boost::asio::error::misc_errors err)
{
    return make_error_code(err);
}

const boost::system::error_code kErrEof = make_err(boost::asio::error::eof);
const boost::system::error_code kErrNoBufs = make_err(boost::system::errc::no_buffer_space);
const boost::system::error_code kErrProto = make_err(boost::system::errc::protocol_error);
const boost::system::error_code kErrTimedOut = make_err(boost::asio::error::timed_out);
} // namespace

NetworkClient::NetworkClient(NetworkHandler *handler) :
    m_handler(handler),
    m_socket(nullptr),
    m_async_timer(),
    m_send_queue(),
    m_disconnect_error(kErrEof)
{
    m_read_buffer.fill(0);
}

NetworkClient::~NetworkClient()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    assert(!is_connected(lock));

    shutdown(lock);
}

void NetworkClient::shutdown(std::unique_lock<std::mutex> &lock)
{
    if(m_socket == nullptr || m_async_timer == nullptr) {
        return;
    }

    auto socket = m_socket;
    auto async_timer = m_async_timer;

    lock.unlock();
    TaskQueue::singleton.enqueue_task([socket, async_timer]() {
        boost::system::error_code ec;
        async_timer->cancel();
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket->close(ec);
    });
    lock.lock();

    m_socket = nullptr;
    m_async_timer = nullptr;
    m_haproxy_handler = nullptr;
}

void NetworkClient::initialize(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                               const NetAddress &remote,
                               const NetAddress &local,
                               const bool haproxy_mode,
                               std::unique_lock<std::mutex> &lock)
{
    if(m_socket) {
        throw std::logic_error("Trying to set a socket of a network client whose socket was already set.");
    }

    assert(std::this_thread::get_id() == g_main_thread_id);

    m_socket = socket;

    boost::asio::ip::tcp::no_delay nodelay(true);
    m_socket->set_option(nodelay);
    boost::asio::socket_base::keep_alive keepalive(true);
    m_socket->set_option(keepalive);

    const int optimal_buffer = 262144;
    boost::asio::socket_base::send_buffer_size send_opt(optimal_buffer);
    boost::asio::socket_base::receive_buffer_size recv_opt(optimal_buffer);
    boost::system::error_code ec;
    m_socket->set_option(send_opt, ec);
    ec.clear();
    m_socket->set_option(recv_opt, ec);

    m_async_timer = std::make_shared<boost::asio::steady_timer>(NetContext::instance().context());

    m_remote = remote;
    m_local = local;
    m_haproxy_mode = haproxy_mode;

    if(m_haproxy_mode) {
        m_haproxy_handler = std::make_unique<HAProxyHandler>();
    } else {
        lock.unlock();
        m_handler->initialize();
        lock.lock();
    }

    lock.unlock();
    start_receive();
    lock.lock();
}

void NetworkClient::send_datagram(DatagramHandle dg)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if(!is_connected(lock)) {
        return;
    }

    m_send_queue.push_back(dg);
    m_total_queue_size += dg->size();

    if(m_total_queue_size > m_max_queue_size && m_max_queue_size != 0) {
        disconnect(kErrNoBufs, lock);
        return;
    }

    if(g_main_thread_id != std::this_thread::get_id()) {
        lock.unlock();
        TaskQueue::singleton.enqueue_task([self = shared_from_this()] () {
            std::unique_lock<std::mutex> guard(self->m_mutex);
            self->flush_send_queue(guard);
        });
    } else {
        flush_send_queue(lock);
    }
}

void NetworkClient::defragment_input(std::unique_lock<std::mutex>& lock)
{
    while(m_data_buf.size() > sizeof(dgsize_t)) {
        dgsize_t data_size = *reinterpret_cast<dgsize_t*>(m_data_buf.data());

        if(data_size == 0) {
            disconnect(kErrProto, lock);
            return;
        }

        if(m_data_buf.size() >= data_size + sizeof(dgsize_t)) {
            size_t total_consumed = sizeof(dgsize_t) + data_size;

            if(total_consumed > m_data_buf.size()) {
                disconnect(kErrProto, lock);
                return;
            }

            DatagramPtr dg = Datagram::create(m_data_buf.data() + sizeof(dgsize_t), data_size);

            size_t overread_size = m_data_buf.size() - total_consumed;
            if(overread_size > 0) {
                m_data_buf = std::vector<uint8_t>(m_data_buf.begin() + total_consumed,
                                                  m_data_buf.end());
            } else {
                m_data_buf.clear();
            }

            lock.unlock();
            m_handler->receive_datagram(dg);
            lock.lock();
        } else {
            return;
        }
    }
}

void NetworkClient::process_datagram(const std::unique_ptr<char[]>& data, size_t size)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_data_buf.empty() && size >= sizeof(dgsize_t)) {
        dgsize_t datagram_size = *reinterpret_cast<dgsize_t*>(data.get());
        if(datagram_size == size - sizeof(dgsize_t)) {
            DatagramPtr dg = Datagram::create(reinterpret_cast<const uint8_t*>(data.get() + sizeof(dgsize_t)),
                                              datagram_size);
            lock.unlock();
            m_handler->receive_datagram(dg);
            return;
        }
    }

    m_data_buf.insert(m_data_buf.end(), data.get(), data.get() + size);
    defragment_input(lock);
}

void NetworkClient::start_receive()
{
    assert(std::this_thread::get_id() == g_main_thread_id);
    schedule_read();
}

void NetworkClient::schedule_read()
{
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        socket = m_socket;
    }
    if(!socket) {
        return;
    }

    auto self = shared_from_this();
    socket->async_read_some(boost::asio::buffer(m_read_buffer),
        [self](const boost::system::error_code &ec, std::size_t bytes) {
            if(ec) {
                self->handle_disconnect(ec);
                return;
            }

            if(bytes == 0) {
                self->schedule_read();
                return;
            }

            auto data = std::make_unique<char[]>(bytes);
            std::memcpy(data.get(), self->m_read_buffer.data(), bytes);

            if(self->m_haproxy_handler != nullptr) {
                size_t bytes_consumed = self->m_haproxy_handler->consume(
                    reinterpret_cast<const uint8_t*>(data.get()), bytes);
                if(bytes_consumed < bytes || bytes_consumed == 0) {
                    if(self->m_haproxy_handler->has_error()) {
                        auto ec = self->m_haproxy_handler->get_error();
                        self->disconnect(ec);
                        self->m_haproxy_handler = nullptr;
                        return;
                    }

                    self->m_is_local = self->m_haproxy_handler->is_local();
                    if(!self->m_is_local) {
                        self->m_local = self->m_haproxy_handler->get_local();
                        self->m_remote = self->m_haproxy_handler->get_remote();
                        self->m_tlv_buf = self->m_haproxy_handler->get_tlvs();
                    }

                    self->m_haproxy_handler = nullptr;
                    self->m_handler->initialize();

                    ssize_t bytes_left = bytes_consumed > 0 ? bytes - bytes_consumed : 0;
                    if(bytes_left > 0) {
                        auto overread = std::make_unique<char[]>(bytes_left);
                        std::memcpy(overread.get(), data.get() + bytes_consumed, bytes_left);
                        self->process_datagram(overread, bytes_left);
                    }
                }
            } else {
                self->process_datagram(data, bytes);
            }

            self->schedule_read();
        });
}

void NetworkClient::disconnect(const boost::system::error_code &ec, std::unique_lock<std::mutex> &lock)
{
    if(m_local_disconnect || m_disconnect_handled) {
        return;
    }

    m_local_disconnect = true;
    m_disconnect_error = ec;

    if(!m_is_sending && m_total_queue_size == 0) {
        shutdown(lock);
    } else {
        if(g_main_thread_id != std::this_thread::get_id()) {
            lock.unlock();
            TaskQueue::singleton.enqueue_task([self = shared_from_this()] () {
                std::unique_lock<std::mutex> guard(self->m_mutex);
                self->flush_send_queue(guard);
            });
        } else {
            flush_send_queue(lock);
        }
    }
}

void NetworkClient::handle_disconnect(const boost::system::error_code &ec, std::unique_lock<std::mutex> &lock)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_disconnect_handled) {
        return;
    }

    m_disconnect_handled = true;

    shutdown(lock);

    lock.unlock();
    if(m_local_disconnect) {
        m_handler->receive_disconnect(NetErrorEvent(m_disconnect_error));
    } else {
        m_handler->receive_disconnect(NetErrorEvent(ec));
    }
    lock.lock();
}

void NetworkClient::flush_send_queue(std::unique_lock<std::mutex> &lock)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(!is_connected(lock) || m_is_sending) {
        return;
    }

    if(m_send_queue.empty()) {
        assert(m_total_queue_size == 0);
        return;
    }

    auto socket = m_socket;
    size_t buffer_size = 0;
    for(const auto &dg : m_send_queue) {
        buffer_size += sizeof(dgsize_t) + dg->size();
    }

    m_send_buf = std::make_unique<char[]>(buffer_size);

    char *send_ptr = m_send_buf.get();
    for(const auto &dg : m_send_queue) {
        dgsize_t len = swap_le(dg->size());
        std::memcpy(send_ptr, reinterpret_cast<char*>(&len), sizeof(dgsize_t));
        send_ptr += sizeof(dgsize_t);

        std::memcpy(send_ptr, dg->get_data(), dg->size());
        send_ptr += dg->size();
        m_total_queue_size -= dg->size();
    }
    m_send_queue.clear();
    assert(send_ptr == m_send_buf.get() + buffer_size);
    assert(m_total_queue_size == 0);

    if(m_write_timeout > 0) {
        m_async_timer->expires_after(std::chrono::milliseconds(m_write_timeout));
        auto self = shared_from_this();
        m_async_timer->async_wait([self](const boost::system::error_code &ec) {
            if(!ec) {
                self->send_expired();
            }
        });
    }

    m_is_sending = true;
    auto self = shared_from_this();
    auto buffer = boost::asio::buffer(m_send_buf.get(), buffer_size);

    lock.unlock();
    boost::asio::async_write(*socket, buffer,
        [self](const boost::system::error_code &ec, std::size_t) {
            if(ec) {
                self->handle_disconnect(ec);
                return;
            }
            self->send_finished();
        });
    lock.lock();
}

void NetworkClient::send_finished()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(!m_is_sending) {
        return;
    }

    m_is_sending = false;
    m_send_buf.reset();

    if(!is_connected(lock)) {
        return;
    }

    m_async_timer->cancel();

    if(m_local_disconnect && m_total_queue_size == 0) {
        shutdown(lock);
        return;
    }

    flush_send_queue(lock);
}

void NetworkClient::send_expired()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(!m_is_sending) {
        return;
    }

    m_is_sending = false;
    m_send_buf.reset();
    m_total_queue_size = 0;
    m_send_queue.clear();

    disconnect(kErrTimedOut, lock);
}
