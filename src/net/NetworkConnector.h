#pragma once
#include <boost/asio/ip/tcp.hpp>
#include "NetTypes.h"

typedef std::function<void(const std::shared_ptr<boost::asio::ip::tcp::socket> &)> ConnectCallback;
typedef std::function<void(const NetErrorEvent& evt)> ConnectErrorCallback;

class NetworkConnector : public std::enable_shared_from_this<NetworkConnector>
{
  public:
    // Parses the string "address" and connects to it. If no port is specified
    // as part of the address, it will use default_port.
    // The provided callback will be invoked with the created socket post-connection.

    NetworkConnector();
    void destroy();
    void connect(const std::string &address, unsigned int default_port,
                 ConnectCallback callback, ConnectErrorCallback err_callback);
  private:
    std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
    ConnectCallback m_connect_callback;
    ConnectErrorCallback m_err_callback;

    void do_connect(const std::string &address, uint16_t port);
};
