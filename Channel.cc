#include "Channel.h"
#include <sys/epoll.h>
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
      : loop_(loop),
        fd_(fd),
        events_(0),
        revents_(0),
        index_(-1),
        tied_(false)
{
}

Channel::~Channel()
{
}

//tie方法什么时候调用？ 
//一个TcpConnection新连接创建的时候 channel握有一个若智能指针 指向上层的TcpConnection对象
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::update()
{
    /*
    通过channel所属的eventloop，调用poller的相应方法  注册fd的events事件
    */
    loop_->updateChannel(this);

}

//在channel所属的eventloop中，把当前的channel删除
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock(); //提升为强智能指针
        if(guard)          //提升成功  当前tie_所观察的对象资源存活
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }

}

//根据poller通知的channel发生的具体事件revents_  
//由channel负责调用具体的回调函数
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    /*
    EPOLLIN - 当关联的文件可以执行 read ()操作时。
    EPOLLOUT - 当关联的文件可以执行 write ()操作时。
    EPOLLRDHUP - (从 linux 2.6.17 开始)当socket关闭的时候，或者半关闭写段的(当使用边缘触发的时候，这个标识在写一些测试代码去检测关闭的时候特别好用)
    EPOLLPRI - 当 read ()能够读取紧急数据的时候。
    EPOLLERR - 当关联的文件发生错误的时候，epoll_wait() 总是会等待这个事件，并不是需要必须设置的标识。
    EPOLLHUP - 当指定的文件描述符被挂起的时候。epoll_wait() 总是会等待这个事件，并不是需要必须设置的标识。当socket从某一个地方读取数据的时候(管道或者socket),这个事件只是标识出这个已经读取到最后了(EOF)。所有的有效数据已经被读取完毕了，之后任何的读取都会返回0(EOF)。
    EPOLLET - 设置指定的文件描述符模式为边缘触发，默认的模式是水平触发。
    EPOLLONESHOT - (从 linux 2.6.17 开始)设置指定文件描述符为单次模式。这意味着，在设置后只会有一次从epoll_wait() 中捕获到事件，之后你必须要重新调用 epoll_ctl() 重新设置。

    */
    //EPOLLHUP 表示读写都关闭
    LOG_INFO("channel handleEvent revents : %d\n", revents_);
    if( (revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if( (revents_ & EPOLLERR) )
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    //EPOLLPRI     外带数据
    if( (revents_ & (EPOLLIN | EPOLLPRI)) )
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if( (revents_ & EPOLLOUT) )
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }


}


