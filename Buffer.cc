#include "Buffer.h"
#include "errno.h"
#include <sys/uio.h>


//从fd上读取数据  poller 工作在 LT 模式
//Buffer缓冲区是有大小的  但从fd上读数据时 不知道tcp数据最终的大小
/*
readv()代表分散读， 即将数据从文件描述符读到分散的内存块中；
writev()代表集中写，即将多块分散的内存一并写入文件描述符中。

ssize_t readv(int fd, const struct iovec *vector, int count); 
ssize_t writev(int fd, const struct iovec *vector, int count);
 
其中，fd参数是被操作的文件描述符。 vector参数是iovec结构体：
 
struct iovec {
    void *iov_base; 指向一个缓冲区，这个缓冲区是存放readv()所接收的数据或 //writev()将要发送的数据
    size_t iov_len; 接收的最大长度以及实际写入的长度
};
 
count参数是vector数组的长度，即有多少块内存需要从fd读出或写到fd。
 
两者调用成功时返回读出/写入fd的字节数，失败返回-1，并设置errno，此时需要引入error.h头文件。
*/

ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0};//栈上内存 64k

    struct iovec vec[2];

    const size_t writable = writableBytes();//Buffer底层缓冲区剩余可写空间的大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = writable < sizeof extrabuf ? 2 : 1;

    const ssize_t n = ::readv(fd, vec, iovcnt);
    if(n < 0)
    {
         *saveErrno = errno;
    }
    else if(n <= writable)  //Buffer的可写缓冲区已经足够存储读出来的数据
    {
        writerIndex_ += n;
    }
    else    //extrabuf也写入了数据
    {
        writerIndex_ = buffer_.size();
        //将暂存在extrabuf中的数据append进Buffer的可写缓冲区中(扩容后)
        append(extrabuf, n - writable);
    }

    return n;

}
