#pragma once
#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <atomic>


class Channel;
class EventLoop;
class Socket;

/*
TcpServer => Acceptor => 有一个新用户连接 通过accept函数拿到connfd

=> TcpConnection 设置回调  => 将connfd打包成 Channel => 注册到Poller  开始监听
=> 有事件发生 执行对应的回调操作
*/

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                const std::string &name,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop* getloop() const  { return loop_; }
    const std::string name() const  { return name_; }
    const InetAddress localAddress() const  { return localAddr_; }
    const InetAddress peerAddress() const  { return peerAddr_; }

    bool connected() const  { return state_ == kConnected; }

    //发送数据
    void send(const std::string &buf);

    //关闭连接
    void shutdown();

    //设置回调
    //当通道有事件时候在Channel::handleEvent()内调用
    void setConnectionCallback(const ConnectionCallback &cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb)
    { highWaterMarkCallback_ = cb; }

    void setCloseCallback(const CloseCallback &cb)
    { closeCallback_ = cb; }

    //建立连接
    void connectEstablished();
    //销毁连接
    void connectDestoryed();


private:
    enum statE { kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(statE state) { state_ = state; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    
    
    //TcpConnection对象和这个EventLoop对象属同一个线程
    EventLoop *loop_; //这里绝对不是baseloop (mainloop)因为TcpConnection都是在subloop中管理的
    //新连接的名字
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    //每个新连接的socket对象
    std::unique_ptr<Socket> socket_;
    //必然会有个Channel通道来处理新连接后的读写事件
    std::unique_ptr<Channel> channel_;

    //本地和对端的地址对象
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;//有新连接时的回调

    //每一次将数据读到inputBuffer时都会调用
    MessageCallback messageCallback_;//有读写消息的回调

    //outBuffer缓冲区的数据全部写入到socketfd内核缓冲区以后会调用
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成的回调

    //当outBuffer缓冲区的大小到达上限值以后会调用
    HighWaterMarkCallback highWaterMarkCallback_;//高水位回调
    CloseCallback closeCallback_;//关闭连接回调

    //用户缓冲区的大小到达一定大小时会做额外的处理：关闭连接。这个是上限值
    size_t HighWaterMark_;

    //从socketfd的读取数据放入的应用层缓冲区
    Buffer inputBuffer_;
    //将应用层缓冲区数据写入到socketfd发送的应用层缓冲区
    Buffer outputBuffer_;

};

