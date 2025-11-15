#include "EventSender.h"

#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>

#include "core/global.h"
#include "net/address_utils.h"
#include "util/NetContext.h"
#include "util/TaskQueue.h"

EventSender::EventSender() : m_log("eventsender", "Event Sender"),
    m_socket(nullptr), m_enabled(false)
{
}

void EventSender::init(const std::string& target)
{
    if(target == "") {
        m_enabled = false;
        m_log.debug() << "Not enabled." << std::endl;
        return;
    }

    m_log.debug() << "Resolving target..." << std::endl;
    auto addresses = resolve_address(target, 7197);

    if(addresses.size() == 0) {
        m_log.fatal() << "Failed to resolve target address " << target << " for EventSender.\n";
        exit(1);
    }

    const auto &addr = addresses.front();
    boost::system::error_code ec;
    auto endpoint_address = boost::asio::ip::make_address(addr.ip, ec);
    if(ec) {
        m_log.fatal() << "Failed to parse resolved address " << addr.ip << " for EventSender: "
                      << ec.message() << "\n";
        exit(1);
    }

    auto &io = NetContext::instance().context();
    m_socket = std::make_shared<boost::asio::ip::udp::socket>(io);
    m_socket->open(endpoint_address.is_v4() ? boost::asio::ip::udp::v4() : boost::asio::ip::udp::v6(), ec);
    if(ec) {
        m_log.fatal() << "Failed to open UDP socket for EventSender: " << ec.message() << "\n";
        exit(1);
    }

    m_target = boost::asio::ip::udp::endpoint(endpoint_address, addr.port);
    m_enabled = true;

    m_log.debug() << "Initialized." << std::endl;
}

void EventSender::send(DatagramHandle dg)
{
    if(!m_enabled) {
        m_log.trace() << "Disabled; discarding event..." << std::endl;
        return;
    }

    m_log.trace() << "Sending event..." << std::endl;

    auto socket = m_socket;
    auto endpoint = m_target;
    auto payload = std::make_shared<std::vector<uint8_t>>(dg->get_data(), dg->get_data() + dg->size());
    auto self = this;

    TaskQueue::singleton.enqueue_task([socket, endpoint, payload, self]() mutable {
        if(!socket) {
            return;
        }

        boost::system::error_code ec;
        socket->send_to(boost::asio::buffer(*payload), endpoint, 0, ec);
        if(ec) {
            self->m_log.warning() << "EventSender send failed: " << ec.message() << std::endl;
        }
    });
}

// And now the convenience class:
LoggedEvent::LoggedEvent()
{
    add("type", "unset");
    add("sender", "unset");
}

LoggedEvent::LoggedEvent(const std::string &type)
{
    add("type", type);
    add("sender", "unset");
}

LoggedEvent::LoggedEvent(const std::string &type, const std::string &sender)
{
    add("type", type);
    add("sender", sender);
}

void LoggedEvent::add(const std::string &key, const std::string &value)
{
    if(m_keys.find(key) == m_keys.end()) {
        m_keys[key] = m_kv.size();
        m_kv.push_back(std::make_pair(key, value));
    } else {
        m_kv[m_keys[key]] = std::make_pair(key, value);
    }
}

static inline void pack_string(DatagramPtr dg, const std::string &str)
{
    size_t size = str.size();

    if(size < 32) {
        // Small enough for fixstr:
        dg->add_uint8(0xa0 + size);
    } else {
        // Use a str16.
        // We don't have to worry about str32, nothing that big will fit in a
        // single UDP packet anyway.
        dg->add_uint8(0xda);
        dg->add_uint8(size >> 8 & 0xFF);
        dg->add_uint8(size & 0xFF);
    }

    dg->add_data(str);
}

DatagramHandle LoggedEvent::make_datagram() const
{
    DatagramPtr dg = Datagram::create();

    // First, append the size of our map:
    size_t size = m_kv.size();
    if(size < 16) {
        // Small enough for fixmap:
        dg->add_uint8(0x80 + size);
    } else {
        // Use a map16.
        // We don't have to worry about map32, nothing that big will fit in a
        // single UDP packet anyway.
        dg->add_uint8(0xde);
        dg->add_uint8(size >> 8 & 0xFF);
        dg->add_uint8(size & 0xFF);
    }

    for(auto &it : m_kv) {
        pack_string(dg, it.first);
        pack_string(dg, it.second);
    }

    return dg;
}
