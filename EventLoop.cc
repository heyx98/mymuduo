#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个Eventloop     __thread->thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeupfd,用来notify唤醒subReactor处理新的Channel
int createEventfd()
{
    /*
    专门用于事件通知的文件描述符（ fd ）
    划重点：eventfd 是一个计数相关的fd。     计数不为零是有可读事件发生，
    read 之后计数会清零，write 则会递增计数器。
    eventfd 对应的文件内容是一个 8 字节的数字，这个数字是 read/write 操作维护的计数


    EFD_CLOEXEC (since Linux 2.6.27)
    文件被设置成 O_CLOEXEC，创建子进程 (fork) 时不继承父进程的文件描述符。
    EFD_NONBLOCK (since Linux 2.6.27)
    文件被设置成 O_NONBLOCK，执行 read / write 操作时，不会阻塞。

    */
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error : %d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
        : looping_(false),
          quit_(false),
          callingPendingFunctors_(false),
          threadId_(CurrentThread::tid()),
          poller_(Poller::newDefaultPoller(this)),
          wakeupFd_(createEventfd()),
          wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exits in this thread %d \n",
                t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    //设置wakeupfd的事件类型和对应的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //eventloop将监听wakeupchannel的EPOLLIN读事件   
    //enableReading()内会updata 向epoll注册事件
    wakeupChannel_->enableReading();

}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;

}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        //监听两类fd  一种是clientfd 一种是wakeupfd mainloop <=>subloop
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_)
        {
            //poller监听哪些channel发生了事件 然后上报给EventLoop 通知Channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /*
        IO线程 mainloop accept fd (channel) ===> subloop
        wakeup subloop后 执行下面的方法 (mainloop事先注册的cb操作)
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_ = false;

}
//退出事件循环 1. loop在自己的线程中调用quit 2.在非loop的线程中 调用loop的quit
void EventLoop::quit()
{
    quit_ = true;
    //如果在其他线程中调用的quit 在一个subloop(woker)中，调用了mainloop(IO)的quit
    if(!isInLoopThread())
    {
        wakeup();
    }

}

//在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())//在当前loop中
    {
        cb();
    }
    else        //在非当前loop线程中执行cb，就需要唤醒loop所在的线程执行cb
    {
        queueInLoop(cb);
    }

}
//把cb放入队列中，唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    //唤醒相应的线程
    //  || callingPendingFunctors_ 的意思是 ： 当前loop正在执行回调 但是loop又有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }

}

//唤醒loop所在的线程  向wakeupfd写一个数据 wakeupchannel发生读事件当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead od 8 \n", n);
    }
}

//Eventloop 的方法 => 调用 Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}



void EventLoop::handleRead()
{
    /*
    1.read函数会从eventfd对应的64位计数器中读取一个8字节的整型变量；

    2.read函数设置的接收buf的大小不能低于8个字节，否则read函数会出错，errno为EINVAL;

    3.read函数返回的值是按小端字节序的；
    */
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",n);
    }
}

//执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;//定义一个局部的functors
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        //和pendingFunctors_交换内容 尽快释放pendingFunctors_
        functors.swap(pendingFunctors_);
    }
    for(const Functor &functor : functors)
    {
        functor();//执行当前loop需要执行的回调
    }
    
    callingPendingFunctors_ = false;

}
