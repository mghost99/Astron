#pragma once
#include <functional>
#include <memory>
#include <boost/asio/ip/tcp.hpp>

#include "NetworkAcceptor.h"
#include "NetTypes.h"

typedef std::function<void(const std::shared_ptr<boost::asio::ip::tcp::socket>&,
                           const NetAddress& remote,
                           const NetAddress& local,
                           const bool haproxy_mode)> TcpAcceptorCallback;

class TcpAcceptor : public NetworkAcceptor
{
  public:
    TcpAcceptor(TcpAcceptorCallback &callback, AcceptorErrorCallback &err_callback);
    virtual ~TcpAcceptor() {}

  private:
    TcpAcceptorCallback m_callback;

    virtual void start_accept();
    void handle_accept(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    void handle_endpoints(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                          const NetAddress& remote,
                          const NetAddress& local);
};
