#pragma once

#include "Poller.h"


#include <vector>
#include <sys/epoll.h>

/*
epoll_create: 创建一个epoll实例 epfd  => epollfd_
epoll_ctl: add modify delete
            int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
epoll_wait: 等待epoll事件从epoll实例中发生， 并返回事件以及对应文件描述符
            int epoll_wait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout);
*/

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    // override  表示这些方法是覆盖方法 
    // 由编译器保证在基类中有这些方法的声明 虚函数
    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;//epoll_wait
    void updateChannel(Channel* channel) override;//epoll_ctl add modify
    void removeChannel(Channel* channel) override;//epoll_ctl delete

private:
    /* data */
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};
