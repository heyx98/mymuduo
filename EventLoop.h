#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

//事件循环类  主要包含了两个大模块
//1. Channel   2. Poller (epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();
    
    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();

    //Eventloop 的方法 => 调用 Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    //判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}

private:
    void handleRead();//wakeup
    void doPendingFunctors();//执行回调

    using ChannelList = std::vector<Channel*>;

    //原子操作 通过CAS实现
    std::atomic_bool looping_; //标志处于loop循环
    std::atomic_bool quit_;     //标志退出loop循环 

    const pid_t threadId_;  //记录创建了当前loop所在线程的tid

    Timestamp pollReturnTime_;  //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    //当mainloop获取到一个新用户的channel,通过轮询算法选择一个subloop,
    //通过wakeupFd_唤醒subloop处理channel上的事件
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    //标识当前的loop是否有在执行回调操作
    std::atomic_bool callingPendingFunctors_;  
    //存储loop需要执行的回调操作
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;  //互斥锁 保护上面的vector容器的线程安全操作





};

