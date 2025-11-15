#include "Timeout.h"
#include <chrono>
#include <boost/system/error_code.hpp>
#include "core/global.h"

Timeout::Timeout(unsigned long ms, std::function<void()> f) :
    m_timer(nullptr),
    m_callback_disabled(false)
{
    initialize(ms, f);
}

Timeout::Timeout() :
    m_timer(nullptr),
    m_callback_disabled(false)
{
}

void Timeout::initialize(unsigned long ms, TimeoutCallback callback)
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    m_timeout_interval = ms;
    m_callback = callback;
}

void Timeout::setup()
{
    assert(m_timer == nullptr);

    m_timer = std::make_shared<boost::asio::steady_timer>(NetContext::instance().context());
}

void Timeout::destroy_timer()
{
    m_callback = nullptr;

    if(m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }

    delete this;
}

void Timeout::timer_callback()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_callback != nullptr && !m_callback_disabled.exchange(true)) {
        m_callback();
    }

    destroy_timer();
}

void Timeout::reset()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_timer == nullptr) {
        setup();
    }

    m_timer->cancel();
    m_timer->expires_after(std::chrono::milliseconds(m_timeout_interval));
    auto self = this;
    m_timer->async_wait([self](const boost::system::error_code &ec) {
        if(!ec) {
            self->timer_callback();
        }
    });
}

bool Timeout::cancel()
{
    const bool already_cancelled = !m_callback_disabled.exchange(true);

    if(std::this_thread::get_id() != g_main_thread_id) {
        TaskQueue::singleton.enqueue_task([self = this]() {
            self->cancel();
        });

        return already_cancelled;
    }

    if(m_timer != nullptr) {
        destroy_timer();
    }

    return already_cancelled;
}

Timeout::~Timeout()
{
    assert(m_callback_disabled.load());
}
