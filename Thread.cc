#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name )
    :   started_(false),
        joined_(false),
        tid_(0),
        func_(std::move(func)),
        name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if(started_ && !joined_)
    {
        thread_->detach();//thread类提供的设置线程分离的方法
    }

}

void Thread::start() //一个Thread对象就是记录了一个新线程的全部信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        //获取新线程的tid
        tid_ = CurrentThread::tid();
        sem_post(&sem);

        func_();        //执行线程函数  => func_()
    }));

    //这里必须等待获取新线程的tid
    sem_wait(&sem);


}

//设置主线程阻塞等待子线程
void Thread::join()
{
    joined_ = true;
    thread_->join();

}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread %d", num);
        name_ = buf;
    }
    
}