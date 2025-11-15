#include "NetContext.h"

NetContext::NetContext() :
    m_io_context(),
    m_work_guard(std::make_unique<WorkGuard>(m_io_context.get_executor()))
{
}

NetContext& NetContext::instance()
{
    static NetContext ctx;
    return ctx;
}

boost::asio::io_context& NetContext::context()
{
    return m_io_context;
}

void NetContext::start(std::size_t background_threads)
{
    if(background_threads == 0 || !m_threads.empty()) {
        m_running = true;
        return;
    }

    if(!m_work_guard) {
        m_work_guard = std::make_unique<WorkGuard>(m_io_context.get_executor());
    }

    for(std::size_t i = 0; i < background_threads; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }

    m_running = true;
}

void NetContext::run()
{
    m_running = true;
    m_io_context.run();
    m_running = false;
}

void NetContext::stop()
{
    if(m_work_guard) {
        m_work_guard->reset();
        m_work_guard.reset();
    }
    m_io_context.stop();
    m_running = false;
}

void NetContext::reset()
{
    m_io_context.restart();
    if(!m_work_guard) {
        m_work_guard = std::make_unique<WorkGuard>(m_io_context.get_executor());
    }
}

void NetContext::join()
{
    for(auto &thread : m_threads) {
        if(thread.joinable()) {
            thread.join();
        }
    }
    m_threads.clear();
}

bool NetContext::running() const
{
    return m_running;
}

