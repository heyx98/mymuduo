#pragma once

#include "noncopyable.h"
#include <functional>
#include "Timestamp.h"
#include <memory>

/*
one loop pre thread
一个线程有一个eventloop  一个eventloop里有一个poller(epoll)
一个poller上可以监听很多个channel   
每个channel属于一个eventloop  一个eventloop会有很多个channel
*/


//前置声明 EventLoop *loop 只定义了类型的指针 前置声明即可   
//Timestamp receiveTime 定义了类对象 所以需要包含头文件
class EventLoop;


/*
Channel理解为通道， 封装了sockfd和其感兴趣的event  如 EPOLLIN  EPOLLOUT事件
还绑定了poller返回的具体事件
对应了Reactor模型的 Demultiplex 事件分发器
*/
class Channel : noncopyable
{
public:
    //两个事件回调
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    //fd得到poller通知后 处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb)  { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb)  { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb)  { errorCallback_ = std::move(cb); }

    //防止当channel被手动remove后， channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    int revents() const { return revents_; }
    void set_revents(int revt) { revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop pre thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    //当改变channel所表示的fd的events事件后，update负责在poller里改变fd相应的epoll_ctl
    //eventloop  => channellist  poller
    void update();
    void handleEventWithGuard(Timestamp receiveTime);


    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; //事件循环
    const int fd_;    //fd,poller监听的对象
    int events_;      //注册fd感兴趣的事件
    int revents_;     //poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为channel通道里面能够获得fd最终发生的具体事件revents 
    //所以channel负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;


};

