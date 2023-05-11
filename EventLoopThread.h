#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>

class EventLoop;
/*
一个EventLoopThread绑定了一个thread和一个loop  
先创建一个thread  再通过新thread创建一个loop 并开启事件循环
*/

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallBack = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallBack &cb = ThreadInitCallBack(), 
            const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();//线程函数  创建loop

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallBack callback_;

};

