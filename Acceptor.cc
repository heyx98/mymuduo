#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>        
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s : %s : %d  listen socket create err: %d \n",
                    __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}


Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
            :   loop_(loop),
                acceptSocket_(createNonblocking()),//创建  socket
                acceptChannel_(loop, acceptSocket_.fd()),
                listenning_(false)


{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);//绑定 bind
    //TcpServer::start()  Acceptor.listen 有新用户连接 
    //执行一个回调 (将与新用户连接的connfd 打包成一个Channel  通过轮询 唤醒一个subloop)
    //baseloop监听到listenfd(channel)有事件发生 读事件 会调用readcallback 运行注册好的回调函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); //listen
    acceptChannel_.enableReading();//acceptChannel_ => poller

}

//listenfd有事件发生 ：有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            //轮询找到subloop 唤醒 分发当前新连接的Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s : %s : %d  accept err: %d \n",
                    __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s : %s : %d sockfd reached limit \n",
                    __FILE__, __FUNCTION__, __LINE__);
        }
    }

}