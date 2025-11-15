#include "TcpAcceptor.h"

#include <boost/asio/error.hpp>

TcpAcceptor::TcpAcceptor(TcpAcceptorCallback &callback, AcceptorErrorCallback& err_callback) :
    NetworkAcceptor(err_callback),
    m_callback(callback)
{
}

void TcpAcceptor::start_accept()
{
    auto self = this;
    auto async_accept = [self]() {
        if(!self->m_started || !self->m_acceptor || !self->m_acceptor->is_open()) {
            return;
        }

        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(self->m_io);
        self->m_acceptor->async_accept(*socket,
            [self, socket](const boost::system::error_code &ec) {
                if(ec) {
                    if(ec != boost::asio::error::operation_aborted && self->m_err_callback) {
                        self->m_err_callback(NetErrorEvent(ec));
                    }

                    if(ec != boost::asio::error::operation_aborted) {
                        self->start_accept();
                    }
                    return;
                }

                self->handle_accept(socket);
                self->start_accept();
            });
    };

    async_accept();
}

void TcpAcceptor::handle_accept(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket)
{
    if(!m_started) {
        boost::system::error_code ec;
        socket->close(ec);
        return;
    }

    boost::system::error_code ec;
    NetAddress remote = make_address(socket->remote_endpoint(ec));
    if(ec) {
        socket->close(ec);
        return;
    }

    NetAddress local = make_address(socket->local_endpoint(ec));
    if(ec) {
        socket->close(ec);
        return;
    }

    handle_endpoints(socket, remote, local);
}

void TcpAcceptor::handle_endpoints(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                                   const NetAddress& remote, const NetAddress& local)
{
    m_callback(socket, remote, local, m_haproxy_mode);
}
