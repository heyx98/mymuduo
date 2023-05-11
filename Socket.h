#pragma once

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        :   sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const  { return sockfd_; }
    // 将服务器本地ip port绑定到listenfd
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);
    
    // tcp socket是全双工通信，这是关闭监听套接字sockfd_写端
    void shutdownWrite();

    void setTcpNoDelay(bool on);//数据直接发送，不做TCP缓冲
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;

};

