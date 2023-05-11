#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>



Socket::~Socket()
{
    close(sockfd_);
}


//绑定sock fd与本地地址addr
void Socket::bindAddress(const InetAddress &localaddr)
{
    /*
    int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen); 
    - 功能:将sockfd 和本地的IP + 端口进行绑定
    - 参数:
        - sockfd : 通过socket函数得到的文件描述符
        - addr : 需要绑定的socket地址,这个地址封装了ip和端口号的信息
        - addrlen : 第二个参数结构体占的内存大小
*/
    if( 0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd : %d  fail \n", sockfd_);
    }

}


//监听本地sock fd
void Socket::listen()
{
    /*
    int listen(int sockfd, int backlog);
    - 功能:监听这个socket上的连接
    - 参数:
        - sockfd : 通过socket()函数得到的文件描述符
        - backlog : 未连接的和已经连接的和的最大值,
*/
    if( 0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd : %d  fail \n", sockfd_);
    }
}

//接受客户端的连接
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr);
    /*
    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    - 功能:接收客户端连接,默认是一个阻塞的函数,阻塞等待客户端连接
    - 参数:
        - sockfd : 用于监听的文件描述符
        - addr : 传出参数,记录了连接成功后客户端的地址信息(ip,port)
        - addrlen : 指定第二个参数的对应的内存大小
    - 返回值:
        - 成功 :用于通信
    */
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}

/*
#include <sys/types.h >
#include <sys/socket.h>
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
--参数：
    sockfd：标识一个套接口的描述字
    level：选项定义的层次；支持SOL_SOCKET、IPPROTO_TCP、IPPROTO_IP和IPPROTO_IPV6
    optname：需设置的选项，而有部分选项需在listen/connect调用前设置才有效，
    这部分选项如下：SO_DEBUG、SO_DONTROUTE、SO_KEEPALIVE、SO_LINGER、SO_OOBINLINE、SO_RCVBUF、
    SO_RCVLOWAT、SO_SNDBUF、SO_SNDLOWAT、TCP_MAXSEG、TCP_NODELAY
    optval：指针，指向存放选项值的缓冲区
    optlen：optval缓冲区长度
*/
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
 /*
SO_REUSEADDR
此参数经常用于当socket正在关闭，处于TIME_WAIT的状态，而此时又想继续重用该socket，
可以启用此参数，否则你在调用bind接口的时候会出现bind failed: Address already in use：
*/
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
