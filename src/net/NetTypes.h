#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <cstdint>
#include <memory>
#include <string>

struct NetAddress
{
    std::string ip;
    std::uint16_t port = 0;

    inline bool operator==(const NetAddress &other) const
    {
        return ip == other.ip && port == other.port;
    }
};

inline NetAddress make_address(const boost::asio::ip::tcp::endpoint &endpoint)
{
    NetAddress addr;
    addr.ip = endpoint.address().to_string();
    addr.port = endpoint.port();
    return addr;
}

inline boost::asio::ip::tcp::endpoint to_endpoint(const NetAddress &addr)
{
    return boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(addr.ip), addr.port);
}

class NetErrorEvent
{
  public:
    NetErrorEvent() = default;
    explicit NetErrorEvent(const boost::system::error_code &ec) : m_code(ec) {}
    explicit NetErrorEvent(int code)
        : m_code(code, boost::system::generic_category())
    {
    }

    inline int code() const
    {
        return m_code.value();
    }

    inline const boost::system::error_code &error_code() const
    {
        return m_code;
    }

    inline std::string message() const
    {
        return m_code.message();
    }

  private:
    boost::system::error_code m_code;
};

using TcpSocket = boost::asio::ip::tcp::socket;
using TcpSocketPtr = std::shared_ptr<TcpSocket>;
using UdpSocket = boost::asio::ip::udp::socket;
using UdpSocketPtr = std::shared_ptr<UdpSocket>;



