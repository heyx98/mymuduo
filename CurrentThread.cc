#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if(t_cachedTid == 0)
        {
            //通过linux系统调用 获取当前线程的tid值
            //第一次调用后就将当前线程Tid存储起来 后续直接从缓存中的变量获取即可
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }

 
}