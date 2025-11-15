#include "NetworkConnector.h"

#include <boost/asio/connect.hpp>
#include <boost/system/error_code.hpp>

#include "address_utils.h"
#include "core/global.h"
#include "util/NetContext.h"

NetworkConnector::NetworkConnector()
{
}

void NetworkConnector::do_connect(const std::string &address,
                                  uint16_t port)
{
    auto endpoints = resolve_address(address, port);

    if(endpoints.empty()) {
        if(m_err_callback) {
            m_err_callback(NetErrorEvent(make_error_code(boost::system::errc::address_not_available)));
        }
        return;
    }

    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(NetContext::instance().context());
    m_socket = socket;

    auto asio_endpoints = std::make_shared<std::vector<boost::asio::ip::tcp::endpoint>>();
    asio_endpoints->reserve(endpoints.size());
    for(const auto &addr : endpoints) {
        asio_endpoints->push_back(to_endpoint(addr));
    }

    auto self = shared_from_this();
    boost::asio::async_connect(*socket, *asio_endpoints,
        [self, asio_endpoints](const boost::system::error_code &ec, const boost::asio::ip::tcp::endpoint &) {
            if(ec) {
                if(self->m_err_callback) {
                    self->m_err_callback(NetErrorEvent(ec));
                }
                return;
            }

            if(self->m_connect_callback) {
                self->m_connect_callback(self->m_socket);
            }
        });
}

void NetworkConnector::destroy()
{
    m_connect_callback = nullptr;
    m_err_callback = nullptr;
    m_socket = nullptr;
}

void NetworkConnector::connect(const std::string &address, unsigned int default_port,
                               ConnectCallback callback, ConnectErrorCallback err_callback)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    m_connect_callback = std::move(callback);
    m_err_callback = std::move(err_callback);

    do_connect(address, default_port);
}
