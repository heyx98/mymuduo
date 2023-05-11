#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <errno.h>
#include <unistd.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s : %s : %d  mainloop is null! :  \n",
                    __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                const std::string &name,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr)
        :   loop_(CheckLoopNotNull(loop)),
            name_(name),
            state_(kConnecting),
            reading_(true),
            socket_(new Socket(sockfd)),
            channel_(new Channel(loop,sockfd)),
            localAddr_(localAddr),
            peerAddr_(peerAddr),
            HighWaterMark_(64*1024*1024)

{
    //给channel设置相应的回调函数 Poller监听到channel感兴趣的事件发生后 会通知channel执行相应的回调操作
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[ %s ] st fd = %d \n", name_.c_str(), sockfd);
    
    //开启Tcp/Ip层的心跳包检测
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[ %s ] st fd = %d state = %d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

//从sockfd中读数据到inputBuffer
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveError = 0;
    //将socketfd中的数据从内核的缓冲区读到inpuBuffer的应用层缓冲区;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveError);
    if(n > 0)
    {
        //已经建立连接的用户 有可读事件发生，调用用户传入的 读事件回调
        //TcpConnection的messageCallback_由TcpServer设置，
        //而TcpServer的又由更上一层（用户应用层）设置。其中如何按协议解包在这个函数内实现。
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveError;
        LOG_ERROR("Tcpconnection::handleRead");
        handleError();
    }
        
}

//从outputBuffer中写数据到sockfd
void TcpConnection::handleWrite()
{
    //如果在关注socketfd内核缓冲区的可写事件的话就执行
    if(channel_->isWriting())
    {
        //内核缓冲区有空间，可以直接调用write从应用层缓冲区写到内核缓冲区
        ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(),
                            outputBuffer_.readableBytes());
        if(n > 0)
        {
            //写入了n个字节就回收应用层的空闲缓冲区空间，实际是下标的移动
            outputBuffer_.retrive(n);
            //说明应用层发送缓冲区已经清空了发送完成
            if(outputBuffer_.readableBytes() == 0) 
            {
                //停止关注可写事件（POLLOUT事件）
                channel_->disableWriting();
                //应用层发送缓冲区没有数据了，回调该函数
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }

                //有可能上层应用已经调用了TcpConnection::shutdown()
                //如果outputBuffer还有数据的话会等数据都发送完了才调用关闭连接；
                //这里就是数据发送完的地方了
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }

            }

        }
        else
        {
            LOG_ERROR("Tcpconnection::handleWrite");
        }

    }
    else
    {
        LOG_ERROR("Tcpconnection fd = %d is down , no more writing \n", 
                    channel_->fd());
        
    }

}


/*
TcpConnection生命管理的步骤：

1.在TcpServer新连接到来时用map<string, std:shared_ptr<TcpConnection>>管理TcpConnection对象，此时新建立的TcpConnection引用计数为1；

2.在TcpConnection::connectEstablished()内将TcpConnection对象指针赋值给其Channel成员的std::weak_ptr<void> tie_。由于是弱指针不增加引用计数，依旧为1；

3.客户端选择关闭连接时：TcpConnection的Channel调用的是读事件:在Channel::handleEvent()函数内将weak_ptr弱指针提升为shared_ptr，引用计数为2；

4.再调用TcpConnection::handleRead()，判断读取到的字节为0，再调用TcpConnection::handleClose()，在这里又会赋值一次shared_ptr<TcpConnection>，引用计数为3；

5.再调用TcpConnection的回调函数closeCallback_为TcpServer::removeConnection()，然后再交给EventLoop函数做最后的处理。

6.最后会调用：TcpServer::removeConnectionInLoop()，在这里将第一步加入map容器内的TcpConnection智能指针移除，引用计数为2；

7.在TcpServer::removeConnectionInLoop()内构造了一个std::bind(&TcpConnection::connectDestroyed, conn)新的可调用对象，引用计数为3；

8.其实开始对函数调用进行出栈返回。结束TcpConnection::handleClose()函数，回收栈空间上的一个智能指针，引用计数为2；

9.结束Channel::handleEvent()函数，回收栈空间上的智能指针，引用计数为1，最后一个引用计数会在EventLoop函数内调用完std::bind(&TcpConnection::connectDestroyed, conn)后，TcpConnection对象被释放。
*/


//poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd = %d state = %d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   //执行连接关闭的回调
    closeCallback_(connPtr);        //关闭连接的回调 执行TcpServer::removeconnection回调

}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0 )
    {
        err =  errno;
    }
    else
    {
        err = optval;
    }

    std::cout << "Tcpconnection::handleError [ " << name_ << " ]  - SO_ERROR = "
            << err << std::endl;
}


void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop, 
                this, 
                buf.c_str(), 
                buf.size()
                ));
        }
    }
}

/*
发送数据 上层应用写得快 内核发送数据慢  需要把待发送的数据写入缓冲区 且设置了高水位回调
*/
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;     //已经发送的数据
    size_t remaining = len; //还未发送的数据
    bool faultError = false;

    //之前调用过该connection的shutdown(), 不能再发送数据
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    /*
    //如果通道（即这个文件描述符）没有关注写事件
    //并且发送缓冲区没有数据。那么可以尝试直接write()
    */
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            //如果所有数据都发送完了（其实是都拷贝到了内核缓冲区）并且回调函数存在  
            //既然这里数据全部发送完成 就不用给channel再注册EPOLLOUT事件了(即 不会触发可写事件回调函数)
            if(remaining == 0 && writeCompleteCallback_)
            {
                //交由EventLoop所在的线程去执行writeCompleteCallback_
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else    //nwrote < 0
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)//由于非阻塞 没有数据 正常返回
            {
                LOG_ERROR("Tcpconnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) //SIGPIP RESET
                {
                    faultError = true;
                }
            }

        } 

    }

    /*
    当前这一次write，并没有将所有数据发送出去，没有一次性将数据拷贝到内核缓冲区
    剩余的数据需要保存到outputBuffer的缓冲区 然后给channel注册EPOLLOUT事件 
    poller发现tcp发送缓冲区有空间,会通知相应的sock channel调用handleWrite回调
    */
    if( !faultError && remaining > 0)
    {
        //当前的发送缓冲区outputBuffer_剩余的待发送的数据长度
        size_t oldLen = outputBuffer_.readableBytes();
        //如果超过了高水位标，那么会回调高水位标的回调函数
        if(oldLen + remaining >= HighWaterMark_ 
            && oldLen < HighWaterMark_//如果oldLen > HighWaterMark_ 之前已经回调过高水位回调函数了
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, 
                        shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);

        //这里要注册channel的可写事件 否则poller不会给channel通知
        //注册后  会回调TcpConnection::handleWrite()函数；
        if(!channel_->isWriting())
        {
            channel_->enableWriting();
        }

    }

}

//建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();//向poller上注册channel的epollin事件

    //新连接建立 执行回调
    connectionCallback_(shared_from_this());

}
//销毁连接
void TcpConnection::connectDestoryed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller中删除掉

}

//关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }

}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  //说明当前output中数据发送完成
    {
        socket_->shutdownWrite(); //socket关闭写端 poller会通知该事件
    }

}