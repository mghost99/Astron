#pragma once

#include <boost/asio/io_context.hpp>
#include <memory>
#include <thread>
#include <vector>

class NetContext
{
  public:
    static NetContext& instance();

    boost::asio::io_context& context();
    void start(std::size_t background_threads = 0);
    void run();
    void stop();
    void reset();
    void join();
    bool running() const;

  private:
    NetContext();
    NetContext(const NetContext&) = delete;
    NetContext& operator=(const NetContext&) = delete;

    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    boost::asio::io_context m_io_context;
    std::unique_ptr<WorkGuard> m_work_guard;
    std::vector<std::thread> m_threads;
    bool m_running = false;
};

