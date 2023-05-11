#pragma once
#include "noncopyable.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

//muduo库中多路事件分发器 核心  io复用模块   虚基类 
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给所有io复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;//epoll_wait
    virtual void updateChannel(Channel *channel) = 0;//epoll_ctl add modify
    virtual void removeChannel(Channel *channel) = 0;//epoll_ctl delete

    //判断参数channel是否在当前poller中
    bool hasChannel(Channel *channel) const;

    //EventLoop事件循环可以通过该接口获取默认的io复用的具体实现对象
    /*
    由于该方法要return一个Poller对象: epoll 或者 poll
    但是return的对象是派生类  
    要想在Poller.cc中实现该方法 就要在基类的头文件中包含派生类EpollPoller.h
    这是不好的设计 ： 在基类头文件中include派生类   基类最好不要引用派生类
    所以该方法单独在DefaultPoller.cc中实现
    */
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    //map的key ： sockfd   value: sockfd所属的channel通道
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    /* data */
    //定义poller所属的事件循环EventLoop
    EventLoop *ownerLoop_;
};

