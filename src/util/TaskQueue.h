#pragma once

#include <functional>

typedef std::function<void()> TaskCallback;

class TaskQueue
{
    public:
        ~TaskQueue();
        static TaskQueue singleton;
        void init_queue();
        void enqueue_task(TaskCallback task);
        void flush_tasks();
};
