#include "InetAddress.h"
#include <strings.h>
#include <string.h>
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
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    // host to net
    addr_.sin_port = htons(port);
    //将字符串转换为in_addr_t类型
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

std::string InetAddress::toIp() const
{
    //addr
    char buf[64] = {0};
    //net to point(点分十进制char*)
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}
std::string InetAddress::toIpPort() const
{
    //ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    /*
    C 库函数 int sprintf(char *str, const char *format, ...) 
    发送格式化输出到 str 所指向的字符串。
    format 标签可被随后的附加参数中指定的值替换，并按需求进行格式化。format 标签属性是
     %[flags][width][.precision][length]specifier  	u 无符号十进制整数
    */
    sprintf(buf+end, ":%u", port);
    return buf;

}
uint16_t InetAddress::toPort() const
{
    // net to host
    return ntohs(addr_.sin_port);
}

//#include <iostream>
//int main()
//{
//    InetAddress addr(8080);
//    std::cout << addr.toIpPort() << std::endl;
//    return 0;
//}