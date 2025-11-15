#include "ClientAgent.h"
#include "ClientFactory.h"

#include "core/global.h"
#include "core/shutdown.h"
#include "core/RoleFactory.h"
#include "dclass/file/hash.h"
#include "net/TcpAcceptor.h"
#include "config/ConfigSection.h"
#include "config/ConfigVariable.h"

#include <optional>
#include <sstream>

using namespace std;

static RoleConfigGroup clientagent_config("clientagent");
static ConfigVariable<std::string> legacy_bind("bind", "0.0.0.0:7198", clientagent_config);
static ConfigVariable<std::string> legacy_version("version", "dev", clientagent_config);
static ConfigVariable<bool> legacy_haproxy("haproxy", false, clientagent_config);
static ConfigVariable<std::string> legacy_manual_hash("manual_dc_hash", "", clientagent_config);
static ConfigGroup legacy_channels("channels", clientagent_config);
static ConfigVariable<channel_t> legacy_channel_min("min", INVALID_CHANNEL, legacy_channels);
static ConfigVariable<channel_t> legacy_channel_max("max", INVALID_CHANNEL, legacy_channels);
static ConfigGroup legacy_client("client", clientagent_config);
static ConfigVariable<bool> legacy_client_relocate("relocate", false, legacy_client);
static ConfigVariable<std::string> legacy_client_interest("add_interest", "visible", legacy_client);
static ConfigVariable<std::string> legacy_client_manual_hash("manual_dc_hash", "", legacy_client);
static ConfigVariable<bool> legacy_client_send_hash("send_hash", true, legacy_client);
static ConfigGroup legacy_tuning("tuning", clientagent_config);
static ConfigVariable<unsigned long> legacy_tuning_interest("interest_timeout", 500, legacy_tuning);

namespace {

std::optional<uint32_t> parse_manual_hash(const std::string &raw, LogCategory *log, const char *source)
{
    if(raw.empty()) {
        return std::nullopt;
    }
    try {
        return static_cast<uint32_t>(std::stoul(raw, nullptr, 0));
    } catch(const std::exception &e) {
        if(log) {
            log->warning() << "Invalid manual_dc_hash '" << raw << "' (" << source
                           << "): " << e.what() << ". Falling back to auto hash.\n";
        }
        return std::nullopt;
    }
}

} // namespace

ClientAgent::ClientAgent(RoleConfig roleconfig) :
    Role(roleconfig),
    m_net_acceptor(nullptr)
{
    ConfigSection config(roleconfig, "roles.clientagent");

    std::string bind_address = config.get_or<std::string>("bind", "0.0.0.0:7198");
    m_server_version = config.get_or<std::string>("version", "dev");
    bool haproxy_mode = config.get_or<bool>("haproxy", false);

    std::stringstream ss;
    ss << "Client Agent (" << bind_address << ")";
    m_log = std::make_unique<LogCategory>("clientagent", ss.str());


    auto parse_hash_node = [this](const YAML::Node &node, const char *source) -> std::optional<uint32_t> {
        if(!node || !node.IsDefined()) {
            return std::nullopt;
        }
        if(!node.IsScalar()) {
            throw std::runtime_error(std::string(source) + " must be a scalar.");
        }
        return parse_manual_hash(node.Scalar(), m_log.get(), source);
    };

    uint32_t config_hash = 0;
    if(auto parsed = parse_hash_node(config.yaml()["manual_dc_hash"], "clientagent.manual_dc_hash")) {
        config_hash = *parsed;
    }

    ConfigSection client_block = config.child("client");
    if(config_hash == 0u) {
        if(auto parsed = parse_hash_node(client_block.yaml()["manual_dc_hash"], "clientagent.client.manual_dc_hash")) {
            config_hash = *parsed;
        }
    }

    if(config_hash > 0u) {
        m_log->info() << "Using manual DC hash override: 0x"
                      << std::hex << config_hash << std::dec << ".\n";
        m_hash = config_hash;
    } else {
        m_hash = dclass::legacy_hash(g_dcf);
        m_log->info() << "No manual_dc_hash provided; using legacy hash 0x"
                      << std::hex << m_hash << std::dec << ".\n";
    }

    m_client_type = client_block.get_or<std::string>("type", "libastron");
    if(!ClientFactory::singleton().has_client_type(m_client_type)) {
        m_log->fatal() << "No client handler exists for type '" << m_client_type << "'.\n";
        exit(1);
    }
    m_client_config = client_block;

    ConfigSection channels = config.child("channels");
    channel_t range_min = channels.get_or<channel_t>("min", INVALID_CHANNEL);
    channel_t range_max = channels.get_or<channel_t>("max", INVALID_CHANNEL);
    m_ct = ChannelTracker(range_min, range_max);

    ConfigSection tuning = config.child("tuning");
    if(tuning.defined()) {
        YAML::Node timeout_node = tuning.yaml()["interest_timeout"];
        if(timeout_node && timeout_node.IsDefined()) {
            if(!timeout_node.IsScalar()) {
                throw std::runtime_error(tuning.path() + ".interest_timeout must be a scalar.");
            }
            try {
                m_interest_timeout = std::stoul(timeout_node.Scalar(), nullptr, 0);
            } catch(const std::exception &e) {
                throw std::runtime_error(tuning.path() + ".interest_timeout: " + e.what());
            }
        } else {
            m_interest_timeout = 500;
        }
    } else {
        m_interest_timeout = 500;
    }

    TcpAcceptorCallback callback = std::bind(&ClientAgent::handle_tcp, this,
                                   std::placeholders::_1,
                                   std::placeholders::_2,
                                   std::placeholders::_3,
                                   std::placeholders::_4);
    AcceptorErrorCallback err_callback = std::bind(&ClientAgent::handle_error, this,
                                            std::placeholders::_1);

    m_net_acceptor = std::unique_ptr<TcpAcceptor>(new TcpAcceptor(callback, err_callback));

    m_net_acceptor->set_haproxy_mode(haproxy_mode);

    // Begin listening for new Clients
    m_net_acceptor->bind(bind_address, 7198);
    m_net_acceptor->start();
}

// handle_tcp generates a new Client object from a raw tcp connection.
void ClientAgent::handle_tcp(const TcpSocketPtr &socket,
                             const NetAddress &remote,
                             const NetAddress &local,
                             const bool haproxy_mode)
{
    m_log->debug() << "Got an incoming connection from "
                   << remote.ip << ":" << remote.port << "\n";

    ClientFactory::singleton().instantiate_client(m_client_type, m_client_config, this, socket, remote, local, haproxy_mode);
}

void ClientAgent::handle_error(const NetErrorEvent& evt)
{
    if(evt.code() == static_cast<int>(boost::system::errc::address_in_use) ||
       evt.code() == static_cast<int>(boost::system::errc::address_not_available)) {
        m_log->fatal() << "Failed to bind to address: " << evt.message() << "\n";
        exit(1);
    }
}

// handle_datagram handles Datagrams received from the message director.
void ClientAgent::handle_datagram(DatagramHandle, DatagramIterator&)
{
    // At the moment, the client agent doesn't actually handle any datagrams
}

static RoleFactoryItem<ClientAgent> ca_fact("clientagent");



/* ========================== *
 *       HELPER CLASSES       *
 * ========================== */
ChannelTracker::ChannelTracker(channel_t min, channel_t max) : m_next(min), m_max(max),
    m_unused_channels()
{
}

channel_t ChannelTracker::alloc_channel()
{
    if(m_next <= m_max) {
        return m_next++;
    } else {
        if(!m_unused_channels.empty()) {
            channel_t c = m_unused_channels.front();
            m_unused_channels.pop();
            return c;
        }
    }
    return 0;
}

void ChannelTracker::free_channel(channel_t channel)
{
    m_unused_channels.push(channel);
}

ConfigSchema ClientAgent::schema()
{
    return Schema::role("clientagent", [](config::SchemaBuilder &builder) {
        builder.allow_additional(false);
        builder.required_string("type");
        builder.required_string("bind");
        builder.required_string("version");
        builder.optional_bool("haproxy");
        builder.optional_string("manual_dc_hash");

        builder.required_object("channels", [](config::SchemaBuilder &channels) {
            channels.allow_additional(false);
            channels.required_int("min");
            channels.required_int("max");
        });

        builder.optional_object("client", [](config::SchemaBuilder &client) {
            client.allow_additional(false);
            client.optional_bool("relocate");
            client.enum_string("add_interest", {"visible", "enabled", "disabled"}, false);
            client.optional_string("manual_dc_hash");
            client.optional_bool("send_hash");
            client.optional_bool("send_version");
        });
        builder.optional_object("tuning", [](config::SchemaBuilder &tuning) {
            tuning.allow_additional(false);
            tuning.optional_int("interest_timeout");
        });
    });
}
