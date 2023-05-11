#include <mymuduo/TcpServer.h>
#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,
            const InetAddress &addr,
            const std::string &name)
        : server_(loop, addr, name),
          loop_(loop)
    {
        //注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, 
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );

        //设置合适的loop数量
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    //连接建立或者断开回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    //可读写事件回调
    void onMessage(const TcpConnectionPtr &conn,
            Buffer *buf,
            Timestamp time)
    {
        std::string msg = buf->retriveAllAsString();
        conn->send(msg);
        conn->shutdown(); //关闭写端 EPOLLHUP => closeCallback_
    }
    EventLoop *loop_;
    TcpServer server_;
};


int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    // EchoServer server => TcpServer server_ => Acceptor acceptor_  (non-blocking listenfd  create bind)
    EchoServer server(&loop, addr, "EchoServer-01");
    server.start();//listen loopthread  listen => acceptChannel => mainloop
    loop.loop(); //启动mainloop的底层poller
    return 0;
}