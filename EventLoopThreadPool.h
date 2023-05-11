#pragma once
#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallBack = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &name);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads)   { numThreads_ = numThreads; }
    void start(const ThreadInitCallBack &cb = ThreadInitCallBack());

    //如果工作在多线程中 baseloop_默认以轮询的方式分配给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const{ return name_; }

private:
    EventLoop *baseLoop_;   
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    //所有创建了事件循环的线程
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    //所有事件循环的指针
    std::vector<EventLoop*> loops_;
    
};

