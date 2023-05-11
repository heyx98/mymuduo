#pragma once
#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    //__thread变量每一个线程有一份独立实体，各个线程的值互不干扰。
    //用来修饰那些带有全局性且值可能变，但是又不值得用全局变量保护的变量。
    //表示每个线程有自己的tid
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        //表示还没有获取过当前线程的tid 
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        
        //通过linux系统调用获取当前线程的tid值后 被存储在t_cachedTid中 
        //t_cachedTid不为零 直接return 不需要再进行系统调用
        return t_cachedTid;
    }
}