#include "MessageDirector.h"
#include <algorithm>
#include <functional>
#include <chrono>
#include <boost/system/error_code.hpp>
#include <boost/icl/interval_bounds.hpp>

#include "core/global.h"
#include "core/msgtypes.h"
#include "config/ConfigVariable.h"
#include "config/constraints.h"
#include "net/TcpAcceptor.h"
#include "MDNetworkParticipant.h"
#include "MDNetworkUpstream.h"

static ConfigGroup md_config("messagedirector");
static ConfigVariable<std::string> bind_addr("bind", "unspecified", md_config);
static ConfigVariable<std::string> connect_addr("connect", "unspecified", md_config);
static ValidAddressConstraint valid_bind_addr(bind_addr);
static ValidAddressConstraint valid_connect_addr(connect_addr);
static ConfigVariable<bool> threaded_mode("threaded", true, md_config);

static ConfigGroup daemon_config("daemon");
static ConfigVariable<std::string> daemon_name("name", "<unnamed>", daemon_config);
static ConfigVariable<std::string> daemon_url("url", "", daemon_config);

MessageDirector MessageDirector::singleton;


MessageDirector::MessageDirector() :  m_initialized(false), m_net_acceptor(nullptr), m_upstream(nullptr),
    m_shutdown(false), m_main_is_routing(false), 
    m_num_threads(std::max(2u, std::thread::hardware_concurrency())),
    m_messages(1024),  // Lock-free queue with 1024 initial capacity
    m_log("msgdir", "Message Director")
{
}

MessageDirector::~MessageDirector()
{
    shutdown_threading();

    m_terminated_participants.insert(m_participants.begin(), m_participants.end());
    m_participants.clear();

    process_terminates();
    
    // Clean up any remaining messages in the lock-free queue
    MessagePair* msg;
    while(m_messages.pop(msg)) {
        delete msg;
    }
}

void MessageDirector::init_network()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(!m_initialized) {
        // Bind to port and listen for downstream servers
        if(bind_addr.get_val() != "unspecified") {
            m_log.info() << "Opening listening socket..." << std::endl;

            TcpAcceptorCallback callback = std::bind(&MessageDirector::handle_connection,
                                           this, std::placeholders::_1);

            AcceptorErrorCallback err_callback = std::bind(&MessageDirector::handle_error,
                                                    this, std::placeholders::_1);

            m_net_acceptor = std::unique_ptr<TcpAcceptor>(new TcpAcceptor(callback, err_callback));
            m_net_acceptor->bind(bind_addr.get_val(), 7199);
            m_net_acceptor->start();
        }

        // Connect to upstream server and start handling received messages
        if(connect_addr.get_val() != "unspecified") {
            m_log.info() << "Connecting upstream..." << std::endl;

            MDNetworkUpstream *upstream = new MDNetworkUpstream(this);

            upstream->connect(connect_addr.get_val());

            m_upstream = upstream;
        }

        if(threaded_mode.get_val()) {
            m_log.info() << "Starting thread pool with " << m_num_threads << " worker threads..." << std::endl;
            
            // Spawn thread pool
            for(size_t i = 0; i < m_num_threads; ++i) {
                m_thread_pool.emplace_back([this, i](std::stop_token stop_token) {
                    this->routing_thread(stop_token, i);
                });
            }
            
            m_cleanup_timer = std::make_shared<boost::asio::steady_timer>(NetContext::instance().context());
            schedule_cleanup();
        }

        m_initialized = true;
    }
}

void MessageDirector::shutdown_threading()
{
    if(m_thread_pool.empty() && !m_cleanup_timer) {
        return;
    }

    // Signal all routing threads to shut down
    m_log.info() << "Shutting down thread pool..." << std::endl;
    m_shutdown.store(true, std::memory_order_release);
    
    // Stop cleanup timer before we destroy any remaining objects
    if(m_cleanup_timer) {
        m_cleanup_timer->cancel();
        m_cleanup_timer.reset();
    }

    // Request all worker threads to stop; jthread will join on destruction
    for(auto &worker : m_thread_pool) {
        worker.request_stop();
    }
    m_thread_pool.clear();
    m_shutdown.store(false, std::memory_order_release);
    
    m_log.info() << "Thread pool shutdown complete." << std::endl;
}

void MessageDirector::route_datagram(MDParticipantInterface *p, DatagramHandle dg)
{
    // Allocate message pair for lock-free queue
    MessagePair* msg = new MessagePair(p, dg);
    
    // Push to lock-free queue - completely wait-free!
    while(!m_messages.push(msg)) {
        // Queue is full (unlikely with 1024 capacity), spin briefly
        std::this_thread::yield();
    }

    if(!m_thread_pool.empty()) {
        // Thread pool is active, workers will pick it up automatically
        return;
    }

    // Non-threaded mode fallback
    if(std::this_thread::get_id() != g_main_thread_id) {
        // We aren't working in threaded mode, but we aren't in the main thread
        // either. For safety, we should post this down to the main thread.
        TaskQueue::singleton.enqueue_task([self = this]() {
            self->flush_queue();
        });
    } else {
        // Main thread: Invoke flush_queue directly.
        flush_queue();
    }
}

void MessageDirector::flush_queue()
{
    // We want to be sure this is being invoked from within the main thread.
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_main_is_routing) {
        // We're already in the middle of a queue flush, return immediately.
        return;
    }

    m_main_is_routing = true;
    
    // Process messages from lock-free queue
    MessagePair* msg;
    while(m_messages.pop(msg)) {
        process_datagram(msg->first, msg->second);
        delete msg;
    }

    // We're done flushing, we can now be invoked from others.
    m_main_is_routing = false;
}

// Thread pool worker function - runs in parallel across multiple threads
void MessageDirector::routing_thread(std::stop_token stop_token, size_t thread_id)
{
    m_log.debug() << "Routing thread " << thread_id << " started" << std::endl;
    
    while(!stop_token.stop_requested() && !m_shutdown.load(std::memory_order_acquire)) {
        MessagePair* msg;
        
        if(m_messages.pop(msg)) {
            // Got a message, process it!
            process_datagram(msg->first, msg->second);
            delete msg;
        } else {
            // Queue is empty, yield to avoid busy-waiting
            // This is much more efficient than condition variables for lock-free queues
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    m_log.debug() << "Routing thread " << thread_id << " exiting" << std::endl;
}

void MessageDirector::schedule_cleanup()
{
    if(!m_cleanup_timer) {
        return;
    }

    m_cleanup_timer->expires_after(std::chrono::milliseconds(50));
    auto timer = m_cleanup_timer;
    timer->async_wait([this, timer](const boost::system::error_code &ec) {
        if(ec || timer != m_cleanup_timer) {
            return;
        }
        this->process_terminates();
        schedule_cleanup();
    });
}

void MessageDirector::process_datagram(MDParticipantInterface *p, DatagramHandle dg)
{
    m_log.trace() << "Processing datagram...." << std::endl;

    std::vector<channel_t> channels;
    DatagramIterator dgi(dg);
    try {
        // Unpack channels to send messages to
        uint8_t channel_count = dgi.read_uint8();
        auto receive_log = m_log.trace();
        receive_log << "Receivers: ";
        for(uint8_t i = 0; i < channel_count; ++i) {
            channel_t channel = dgi.read_channel();
            receive_log << channel << ", ";
            channels.push_back(channel);
        }
        receive_log << "\n";
    } catch(const DatagramIteratorEOF &) {
        // Log error with receivers output
        if(p) {
            m_log.error() << "Detected truncated datagram reading header from '"
                          << p->m_name << "'.\n";
        } else {
            m_log.error() << "Detected truncated datagram reading header from "
                          "unknown participant.\n";
        }
        return;
    }

    // Find the participants that need to receive the message
    std::unordered_set<ChannelSubscriber*> receiving_participants;
    lookup_channels(channels, receiving_participants);
    if(p) {
        receiving_participants.erase(p);
    }

    // Send the datagram to each participant
    // Make a copy to avoid iterator invalidation if participant terminates during handling
    std::vector<MDParticipantInterface*> participants_to_notify;
    participants_to_notify.reserve(receiving_participants.size());
    for(const auto& it : receiving_participants) {
        participants_to_notify.push_back(static_cast<MDParticipantInterface *>(it));
    }
    
    for(auto participant : participants_to_notify) {
        // Check if participant is still valid (not terminated)
        if(participant->is_terminated()) {
            continue;
        }
        
        DatagramIterator msg_dgi(dg, dgi.tell());

        try {
            participant->handle_datagram(dg, msg_dgi);
        } catch(const DatagramIteratorEOF &) {
            // Log error with receivers output
            if(p) {
                m_log.error() << "Detected truncated datagram in handle_datagram for '"
                              << participant->m_name << "' from participant '" << p->m_name << "'.\n";
            } else {
                m_log.error() << "Detected truncated datagram in handle_datagram for '"
                              << participant->m_name << "' from unknown participant.\n";
            }
            return;
        }
    }

    // Send message upstream, if necessary
    if(p && m_upstream) {
        m_upstream->handle_datagram(dg);
        m_log.trace() << "...routing upstream." << std::endl;
    } else if(!p) {
        // If there is no participant, then it came from the upstream
        m_log.trace() << "...not routing upstream: It came from there." << std::endl;
    } else {
        // Otherwise this is the root MessageDirector.
        m_log.trace() << "...not routing upstream: There is none." << std::endl;
    }

    // N.B. Participants may reach end-of-life after receiving a datagram, or may
    // be terminated in another thread (for example if a network socket closes);
    // In single-threaded mode, process terminates immediately.
    // In multi-threaded mode, terminates are processed periodically to avoid
    // deleting participants while worker threads might be using them.
    if(m_thread_pool.empty()) {
        // Single-threaded mode - safe to process immediately
        process_terminates();
    }
    // In threaded mode, terminates are processed by the periodic cleanup task
}


void MessageDirector::process_terminates()
{
    std::unordered_set<MDParticipantInterface*> terminating_participants;

    {
        std::lock_guard<std::mutex> lock(m_terminated_lock);
        terminating_participants = std::move(m_terminated_participants);
    }

    for(const auto& it : terminating_participants) {
        delete it;
    }
}

void MessageDirector::on_add_channel(channel_t c)
{
    if(m_upstream) {
        // Send upstream control message
        m_upstream->subscribe_channel(c);
    }
}

void MessageDirector::on_remove_channel(channel_t c)
{
    if(m_upstream) {
        // Send upstream control message
        m_upstream->unsubscribe_channel(c);
    }
}

void MessageDirector::on_add_range(channel_t lo, channel_t hi)
{
    if(m_upstream) {
        // Send upstream control message
        m_upstream->subscribe_range(lo, hi);
    }
}

void MessageDirector::on_remove_range(channel_t lo, channel_t hi)
{
    if(m_upstream) {
        // Send upstream control message
        m_upstream->unsubscribe_range(lo, hi);
    }
}

void MessageDirector::handle_connection(const TcpSocketPtr &socket)
{
    if(!socket) {
        return;
    }

    boost::system::error_code ec;
    NetAddress remote;
    auto endpoint = socket->remote_endpoint(ec);
    if(!ec) {
        remote = make_address(endpoint);
    }

    m_log.info() << "Got an incoming connection from "
                 << remote.ip << ":" << remote.port << std::endl;
    new MDNetworkParticipant(socket); // Deletes itself when connection is lost
}

void MessageDirector::handle_error(const NetErrorEvent& evt)
{
    if(evt.code() == static_cast<int>(boost::system::errc::address_in_use) ||
       evt.code() == static_cast<int>(boost::system::errc::address_not_available)) {
        m_log.fatal() << "Failed to bind to address: " << evt.message() << "\n";
        exit(1);
    }
}

void MessageDirector::add_participant(MDParticipantInterface* p)
{
    std::lock_guard<std::mutex> lock(m_participants_lock);
    m_participants.insert(p);
}

void MessageDirector::remove_participant(MDParticipantInterface* p)
{
    // Unsubscribe the participant from any remaining channels
    unsubscribe_all(p);

    // Stop tracking participant
    {
        std::lock_guard<std::mutex> lock(m_participants_lock);
        m_participants.erase(p);
    }

    // Send out any post-remove messages the participant may have added.
    // N.B. this is done last, because we don't want to send messages
    // through the Director while a participant is being removed, as
    // certain data structures may not have their invariants satisfied
    // during that time.
    p->post_remove();

    // Mark the participant for deletion
    {
        std::lock_guard<std::mutex> lock(m_terminated_lock);
        m_terminated_participants.insert(p);
    }
}

void MessageDirector::preroute_post_remove(channel_t sender, DatagramHandle post_remove)
{
    // Add post remove upstream
    if(m_upstream != nullptr) {
        DatagramPtr dg = Datagram::create(CONTROL_ADD_POST_REMOVE);
        dg->add_channel(sender);
        dg->add_blob(post_remove);
        m_upstream->handle_datagram(dg);
    }
}

void MessageDirector::recall_post_removes(channel_t sender)
{
    // Clear post removes upstream
    if(m_upstream != nullptr) {
        DatagramPtr dg = Datagram::create(CONTROL_CLEAR_POST_REMOVES);
        dg->add_channel(sender);
        m_upstream->handle_datagram(dg);
    }
}

void MessageDirector::receive_datagram(DatagramHandle dg)
{
    route_datagram(nullptr, dg);
}

void MessageDirector::receive_disconnect(const NetErrorEvent &evt)
{
    m_log.fatal() << "Lost connection to upstream md: " << evt.message() << std::endl;
    exit(1);
}