#include "TaskQueue.h"
#include "NetContext.h"
#include <boost/asio/post.hpp>

TaskQueue TaskQueue::singleton;

TaskQueue::~TaskQueue()
{
}

void TaskQueue::init_queue()
{
}

void TaskQueue::enqueue_task(TaskCallback task)
{
    boost::asio::post(NetContext::instance().context(), std::move(task));
}

void TaskQueue::flush_tasks()
{
}
