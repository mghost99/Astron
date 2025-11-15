#include <ctime>
#include <cctype>

#include "core/RoleFactory.h"
#include "config/constraints.h"
#include "EventLogger.h"
#include "util/EventSender.h"
#include "util/NetContext.h"

#include "msgpack_decode.h"

static RoleConfigGroup el_config("eventlogger");
static ConfigVariable<std::string> bind_addr("bind", "0.0.0.0:7197", el_config);
static ConfigVariable<std::string> output_format("output", "events-%Y%m%d-%H%M%S.log", el_config);
static ConfigVariable<std::string> rotate_interval("rotate_interval", "0", el_config);
static ValidAddressConstraint valid_bind_addr(bind_addr);

EventLogger::EventLogger(RoleConfig roleconfig) : Role(roleconfig),
    m_log("eventlogger", "Event Logger"), m_file(nullptr)
{
    bind(bind_addr.get_rval(roleconfig));

    m_file_format = output_format.get_rval(roleconfig);
    open_log();

    LoggedEvent event("log-opened", "EventLogger");
    event.add("msg", "Log opened upon Event Logger startup.");
    process_packet(event.make_datagram(), m_local);
}

void EventLogger::bind(const std::string &addr)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    m_log.info() << "Opening UDP socket..." << std::endl;
    auto addresses = resolve_address(addr, 7197);

    if(addresses.size() == 0) {
        m_log.fatal() << "Failed to bind to EventLogger address " << addr << "\n";
        exit(1);
    }

    m_local = addresses.front();

    boost::system::error_code ec;
    auto endpoint_address = boost::asio::ip::make_address(m_local.ip, ec);
    if(ec) {
        m_log.fatal() << "Failed to parse EventLogger address " << m_local.ip << ": " << ec.message() << "\n";
        exit(1);
    }

    auto &io = NetContext::instance().context();
    m_socket = std::make_shared<boost::asio::ip::udp::socket>(io);
    boost::asio::ip::udp::endpoint endpoint(endpoint_address, m_local.port);
    m_socket->open(endpoint.protocol(), ec);
    if(ec) {
        m_log.fatal() << "Failed to open UDP socket for EventLogger: " << ec.message() << "\n";
        exit(1);
    }

    m_socket->bind(endpoint, ec);
    if(ec) {
        m_log.fatal() << "Failed to bind UDP socket for EventLogger: " << ec.message() << "\n";
        exit(1);
    }

    start_receive();
}

void EventLogger::open_log()
{
    time_t rawtime;
    time(&rawtime);

    char filename[1024];
    strftime(filename, 1024, m_file_format.c_str(), localtime(&rawtime));
    m_log.debug() << "New log filename: " << filename << std::endl;

    if(m_file) {
        m_file->close();
    }

    m_file.reset(new std::ofstream(filename));

    m_log.info() << "Opened new log." << std::endl;
}

void EventLogger::cycle_log()
{
    open_log();

    LoggedEvent event("log-opened", "EventLogger");
    event.add("msg", "Log cycled.");
    process_packet(event.make_datagram(), m_local);
}

void EventLogger::process_packet(DatagramHandle dg, const NetAddress& sender)
{
    DatagramIterator dgi(dg);
    std::stringstream stream;

    try {
        msgpack_decode(stream, dgi);
    } catch(const DatagramIteratorEOF&) {
        m_log.error() << "Received truncated packet from "
                      << sender.ip << ":" << sender.port << std::endl;
        return;
    }

    if(dgi.tell() != dg->size()) {
        m_log.error() << "Received packet with extraneous data from "
                      << sender.ip << ":" << sender.port << std::endl;
        return;
    }

    std::string data = stream.str();
    m_log.trace() << "Received: " << data << std::endl;

    // This is a little bit of a kludge, but we should make sure we got a
    // MessagePack map as the event log element, and not some other type. The
    // easiest way to do this is to make sure that the JSON representation
    // begins with {
    if(data[0] != '{') {
        m_log.error() << "Received non-map event log from "
                      << sender.ip << ":" << sender.port
                      << ": " << data << std::endl;
        return;
    }

    // Now let's insert our timestamp:
    time_t rawtime;
    time(&rawtime);

    char timestamp[64];
    strftime(timestamp, 64, "{\"_time\": \"%Y-%m-%d %H:%M:%S%z\", ", localtime(&rawtime));

    *m_file.get() << timestamp << data.substr(1) << std::endl;
}

void EventLogger::start_receive()
{
    if(!m_socket) {
        return;
    }

    auto buffer = std::make_shared<std::vector<uint8_t>>(65536);
    auto sender_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();

    m_socket->async_receive_from(boost::asio::buffer(*buffer), *sender_endpoint,
        [this, buffer, sender_endpoint](const boost::system::error_code &ec, std::size_t bytes) {
            if(!ec && bytes > 0) {
                m_log.trace() << "Got packet from " << sender_endpoint->address().to_string()
                              << ":" << sender_endpoint->port() << ".\n";

                DatagramPtr dg = Datagram::create(buffer->data(), bytes);
                NetAddress sender;
                sender.ip = sender_endpoint->address().to_string();
                sender.port = sender_endpoint->port();
                process_packet(dg, sender);
            } else if(ec && ec != boost::asio::error::operation_aborted) {
                m_log.warning() << "EventLogger receive error: " << ec.message() << std::endl;
            }

            start_receive();
        });
}

static RoleFactoryItem<EventLogger> el_fact("eventlogger");
