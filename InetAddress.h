#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

//封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
        {}
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in &addr)   { addr_ = addr; }
private:
/*
    struct sockaddr_in
    {
        sa_family_t sin_family;     地址族类型变量，
                                    与协议族类型对应（PF_INET 表示 IPv4 协议，PF_INET6 表示 IPv6 协议族）
        in_port_t  sin_port;        Port number.
        struct in_addr sin_addr;    Internet address.
        unsigned char xxx;          用来和 struct sockaddr 保持大小相等的
    };
    struct in_addr
    {
        in_addr_t s_addr;
    };
*/
    sockaddr_in addr_;
};
