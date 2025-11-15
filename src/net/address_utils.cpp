#include "address_utils.h"

#include <algorithm>
#include <utility>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include "util/NetContext.h"

static bool split_port(std::string &ip, uint16_t &port)
{
    size_t last_colon = ip.rfind(':');
    if(last_colon == std::string::npos) {
        return true;
    }

    // If the input contains *multiple* colons, it's an IPv6 address.
    // We ignore these unless the IPv6 address is bracketed and the port
    // specification occurs outside of the brackets.
    // (e.g. "[::]:1234")
    if(std::count(ip.begin(), ip.end(), ':') > 1) {
        // Yep, we're assuming IPv6. Let's see if the last colon has a
        // ']' before it.
        // Note that this still can lead to weird inputs getting
        // through, but that'll get caught by a later parsing step. :)
        if(ip[last_colon - 1] != ']') {
            return true;
        }
    }

    try {
        port = std::stoi(ip.substr(last_colon + 1));
        ip = ip.substr(0, last_colon);
    } catch(const std::invalid_argument&) {
        return false;
    }

    return true;
}

static std::pair<bool, NetAddress> parse_address(const std::string &ip, uint16_t port)
{
    std::string cleaned = ip;
    if(!cleaned.empty() && cleaned.front() == '[' && cleaned.back() == ']') {
        cleaned = cleaned.substr(1, cleaned.length() - 2);
    }

    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(cleaned, ec);
    if(ec) {
        return std::make_pair(false, NetAddress{});
    }

    NetAddress addr;
    addr.ip = address.to_string();
    addr.port = port;
    return std::make_pair(true, addr);
}

static bool validate_hostname(const std::string &hostname)
{
    // Standard hostname rules (a la RFC1123):
    // - Must consist of only the characters: A-Z a-z 0-9 . -
    // - Each dot-separated section must contain at least one character.
    // - The - character may not appear twice.
    // - Hostname sections may not begin or end with a -
    //
    // In other words, we must not have a "..", "-.", or ".-" anywhere.
    // We also have to verify that the beginning of the hostname is not a
    // "-" or "." and that the end is not a "-".
    // Everything else is accepted as a valid hostname.

    for(auto it = hostname.begin(); it != hostname.end(); ++it) {
        if(*it != '.' && *it != '-' && !isalnum(*it)) {
            return false;
        }
    }

    if(hostname.find("..") != std::string::npos ||
       hostname.find("-.") != std::string::npos ||
       hostname.find(".-") != std::string::npos) {
        return false;
    }

    if(hostname[0] == '-' || hostname[0] == '.' || hostname[0] == '\0') {
        return false;
    }

    if(hostname[hostname.length() - 1] == '-') {
        return false;
    }

    return true;
}

bool is_valid_address(const std::string &hostspec)
{
    std::string host = hostspec;
    uint16_t port = 0;

    if(!split_port(host, port)) {
        return false;
    }

    auto result = parse_address(host, port);

    if(result.first) {
        return true;
    } else {
        return validate_hostname(host);
    }
}

std::vector<NetAddress> resolve_address(const std::string &hostspec, uint16_t port)
{
    std::vector<NetAddress> ret;

    std::string host = hostspec;

    if(!split_port(host, port)) {
        return ret;
    }

    auto result = parse_address(host, port);
    if(result.first) {
        ret.push_back(result.second);
        return ret;
    }

    auto &io = NetContext::instance().context();
    boost::asio::ip::tcp::resolver resolver(io);
    boost::system::error_code ec;
    auto results = resolver.resolve(host, std::to_string(port), ec);
    if(ec) {
        return ret;
    }

    for(const auto &entry : results) {
        ret.push_back(make_address(entry.endpoint()));
    }

    return ret;
}
