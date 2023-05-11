#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallBack &cb, 
        const std::string &name)
        :   loop_(nullptr),
            exiting_(false),
            thread_(std::bind(&EventLoopThread::threadFunc, this), name),
            mutex_(),
            cond_(),
            callback_(cb)
                
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

/*
当调用EventLoopThread::startLoop()的时候 才会用thread_来调用Thread::start()
此时才会创建新的线程  这个刚刚创建的新线程需要执行传给thread_的线程函数threadFunc()
threadFunc()是在构造成员变量thread_时传入的参数
等新创建的线程初始化成员变量EventLoop *loop_完成后  
才会通知调用EventLoopThread::startLoop()的线程 访问成员变量loop_


即： 调用EventLoopThread::startLoop()就会返回一个EventLoop*对象  
    并且成员变量EventLoop *loop_也记录了该EventLoop的地址
*/
EventLoop* EventLoopThread::startLoop()
{
    thread_.start();//启动创建底层的线程  执行 传给thread_的线程函数threadFunc()

    EventLoop *loop = nullptr; //用来返回的变量
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while( loop_ == nullptr)
        {
            cond_.wait(lock);//等待新创建的线程初始化成员变量EventLoop *loop_完成
        }
        loop = loop_;  
    }
    return loop;

}

//该方法是单独在新线程中运行的
void EventLoopThread::threadFunc()
{
    //创建一个独立的EventLoop，和上面的线程是一一对应的 one loop pre thread
    EventLoop loop;

    if(callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;//把新线程创建的loop对象的地址传给成员变量 loop_
        cond_.notify_one();//通知调用EventLoopThread::startLoop()的线程 loop_已经赋值
    }

    //执行EventLoop.loop 开启了底层线程的Poller
    //开始进入阻塞状态来监听用户的连接或者读写时间
    loop.loop();//EventLoop.loop => Poller.poll

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;


}
