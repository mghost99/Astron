#include "NetworkAcceptor.h"
#include "address_utils.h"
#include "core/global.h"
#include "util/NetContext.h"
#include <boost/system/error_code.hpp>

NetworkAcceptor::NetworkAcceptor(AcceptorErrorCallback err_callback) :
    m_io(NetContext::instance().context()),
    m_acceptor(std::make_unique<boost::asio::ip::tcp::acceptor>(m_io)),
    m_started(false),
    m_haproxy_mode(false),
    m_err_callback(err_callback)
{
}

void NetworkAcceptor::bind(const std::string &address,
        unsigned int default_port)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    auto addresses = resolve_address(address, default_port);

    if(addresses.empty()) {
        m_err_callback(NetErrorEvent(make_error_code(boost::system::errc::address_not_available)));
        return;
    }

    boost::system::error_code last_error;
    bool bound = false;

    for(const auto &addr : addresses) {
        boost::asio::ip::tcp::endpoint endpoint = to_endpoint(addr);
        last_error.clear();

        m_acceptor->close();
        m_acceptor->open(endpoint.protocol(), last_error);
        if(last_error) {
            continue;
        }

        boost::asio::socket_base::reuse_address reuse(true);
        m_acceptor->set_option(reuse, last_error);
        if(last_error) {
            continue;
        }

        m_acceptor->bind(endpoint, last_error);
        if(last_error) {
            continue;
        }

        bound = true;
        break;
    }

    if(!bound) {
        m_err_callback(NetErrorEvent(last_error));
        return;
    }
}

void NetworkAcceptor::start()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_started) {
        // Already started, start() was called twice!
        return;
    }

    m_started = true;
    
    // Queue listener for loop.
    boost::system::error_code ec;
    m_acceptor->listen(boost::asio::socket_base::max_listen_connections, ec);
    if(ec) {
        m_err_callback(NetErrorEvent(ec));
        return;
    }

    start_accept();
}

void NetworkAcceptor::stop()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(!m_started) {
        // Already stopped, stop() was called twice!
        return;
    }

    m_started = false;

    boost::system::error_code ec;
    m_acceptor->close(ec);
}
