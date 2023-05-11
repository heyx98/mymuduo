#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"
#include <strings.h>

EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s : %s : %d  mainloop is null! :  \n",
                    __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
            const InetAddress &listenAddr,
            const std::string &name,
            Option option)
            : loop_(CheckLoopNotNull(loop)),
              ipPort_(listenAddr.toIpPort()),
              name_(name),
              acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
              threadPool_(new EventLoopThreadPool(loop, name)),
              connectionCallback_(),
              messageCallback_(),
              nextConnId_(1),
              started_(0)

{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
            std::placeholders::_1, std::placeholders::_2));
}


TcpServer::~TcpServer()
{
    for(auto& item : connections_)
    {
        //这个局部的shared_ptr智能指针对象 出作用域可以自动释放new出来的TcpConnection对象资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        
        conn->getloop()->runInLoop(
            std::bind(&TcpConnection::connectDestoryed, conn)
            );
    }

}

//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}
    
//开启服务器监听 => 开启mainloop start => 开启 Acceptor.listen
void TcpServer::start()
{
    if(started_++ == 0 ) //防止一个TcpServer被start多次
    {
        threadPool_->start(threadInitCallback_);//启动底层loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}


//有一个新客户端的连接 会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    //轮询算法 选择一个subloop 来管理这个新channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, " -%s#%d ", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());


    //通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getsockname");
    }
    InetAddress localAddr(local);

    //根据连接成功的sockfd创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
                                ioLoop,
                                connName,
                                sockfd,
                                localAddr,
                                peerAddr));
    connections_[connName] = conn;

//下面的回调都是用户设置给TcpServer => TcpConnection => Channel => Poller => notify channel调用
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置如何关闭连接的回调 conn->shutdown
    conn->setCloseCallback(std::bind(
        &TcpServer::removeConnection, this, std::placeholders::_1));
    
    //直接调用TcpConnection::connectEstablished()方法
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}


void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n",
        name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getloop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestoryed, conn)
    );
}