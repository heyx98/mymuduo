#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

//对应channel中的成员变量  index_ = -1
//channel不在ChannelMap中 
const int kNew = -1;
//channel在ChannelMap中且已注册进poller中
const int kAdded = 1;
//channel从poller删除但仍在ChannelMap中 
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) //vector<epoll_event>
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error : %d \n", errno);
    }

}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}

//epoll_wait
//int epoll_wait(int epfd, struct epoll_event *events,
//                      int maxevents, int timeout);
Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) 
{
    //用LOG_DEBUG更合理
    LOG_INFO("func = %s => fd total count= %lu \n",
            __FUNCTION__, channels_.size());
    //events_用于回传待处理事件的数组
    //events_.begin()先解引用 * 得到首元素，再对首元素取地址 &
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), 
                                static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;

    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO(" %d events happend \n", numEvents);

        fillActiveChannels(numEvents, activeChannels);

        if(numEvents == events_.size()) //扩容
        {
            events_.resize(events_.size() * 2);
        }
    }
    //没有事件发生  只是因为timeout时间到了 
    else if(0 == numEvents)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll() err!");

        }
    }

    return now;

}

//epoll_ctl add modify
//channel update remove  => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/*
                EventLoop
    ChannelList             Poller
                            ChannelMap <fd, Channel*>  epollfd
*/
void EpollPoller::updateChannel(Channel *channel) 
{
    const int index = channel->index();
    LOG_INFO("func = %s => fd = %d events = %d index = %d \n",
            __FUNCTION__, channel->fd(), channel->events(), channel->index());
            
    if(index == kNew || index == kDeleted)
    {
        if(index == kNew) //channel不在ChannelMap中 
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded); 
        update(EPOLL_CTL_ADD, channel);
    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }

    }

}


//1.epoll_ctl delete 2. 从ChannelMap中移除 
void EpollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func = %s => fd = %d events = %d index = %d \n",
            __FUNCTION__, channel->fd(), channel->events(), channel->index());
    
    int index = channel->index();
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);


}

//填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for(int i = 0; i < numEvents; ++i)
    {
        //LOG_INFO(" fillActiveChannels i = %d \n", i);
        //此时已经将所有就绪的事件从内核事件表中复制到events_指向的数组中
        auto it = channels_.find(events_[i].data.fd);
        Channel* channel = it->second;
        //Channel* channel = static_cast<Channel*>(events_[i].data.ptr);

        //std::cout << typeid(channel).name() << std::endl;
        //std::cout << channel << " " << events_[i].data.ptr;
        /*LOG_INFO(" fillActiveChannels events_[i].events: %d channel->revents_: %d\n"
        ,events_[i].events, channel->revents());*/
        channel->set_revents(events_[i].events);
        //EventLoop拿到了Poller返回给它的所有发生事件的channel列表
        activeChannels->push_back(channel);
    }

}
//更新channel通道
//int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
void EpollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    int fd = channel->fd();
    event.events = channel->events();
    event.data.ptr = channel;
    event.data.fd = fd;
    
    /*
    通过epoll_ctl函数添加进来的事件event都会被放在红黑树的某个节点内
    当相应的事件发生后，就会调用回调函数：ep_poll_callback,
    这个回调函数就所把这个事件添加到rdllist这个双向链表中。
    当我们调用epoll_wait时，epoll_wait只需要检查rdlist双向链表中是否有存在注册的事件
    将取得的events和相应的fd发送到用户空间（封装在struct epoll_event，从epoll_wait返回）。  
    */
    
    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl DEL error : %d \n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl ADD/MOD error : %d \n", errno);
        }
    }

}