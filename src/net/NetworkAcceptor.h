#pragma once
#include <thread>
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include "NetTypes.h"
#include "util/NetContext.h"

typedef std::function<void(const NetErrorEvent& evt)> AcceptorErrorCallback;

class NetworkAcceptor
{
  public:
    virtual ~NetworkAcceptor() {}

    // Parses the string "address" and binds to it. If no port is specified
    // as part of the address, it will use default_port.
    void bind(const std::string &address, unsigned int default_port);

    void start();
    void stop();

    inline void set_haproxy_mode(bool haproxy_mode)
    {
        m_haproxy_mode = haproxy_mode;
    }

  protected:
    boost::asio::io_context &m_io;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;

    bool m_started = false;
    bool m_haproxy_mode = false;
    AcceptorErrorCallback m_err_callback;

    NetworkAcceptor(AcceptorErrorCallback err_callback);

    virtual void start_accept() = 0;
};
