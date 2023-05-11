#pragma once

#include <vector>
#include <string>
#include <algorithm>
//网络库底层的缓冲区
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        :   buffer_(kCheapPrepend + initialSize),
            readerIndex_(kCheapPrepend),
            writerIndex_(kCheapPrepend)
    {}
    
    size_t readableBytes() const    { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const    { return buffer_.size() -  writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const    { return begin() + readerIndex_; }

    //
    void retrive(size_t len)
    {
        if(len < readableBytes())
        {
            //只读取了可读缓冲区数据的一部分(len) 还剩readerIndex_ += len -- writerIndex_没读
            readerIndex_ += len;  
        }
        else    //len == readableBytes()
        {
            retriveAll();
        }

    }

    void retriveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    
    //把onMessage函数上报的Buffer数据 ==> string 类型 
    std::string retriveAllAsString()
    {
        return retriveAsString(readableBytes());//可读取数据的长度
    }

    std::string retriveAsString(size_t len)
    {
        std::string result(peek(), len);
        ///上条语句已经将缓冲区中要读取的数据读取（转成string类型return了, 
        //接下来要对缓冲区进行复位操作
        retrive(len); 
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        if(writableBytes() < len)
        {
            makespace(len);//扩容函数
        }
    }

    //把 [data , data + len] 内存上的数据 copy到 buffer的writable缓冲区域内
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite()  { return begin() + writerIndex_; }
    const char* beginWrite() const  { return begin() + writerIndex_; }
    
    //从fd上读取数据
    ssize_t readFd(int fd, int* saveError);
    //从fd上fasong数据
    ssize_t writeFd(int fd, int* saveError);


private:
    //1. it.begin() vector首元素的迭代器  2.it.operator*() 取值运算符重载 首元素 3. & 取址
    char* begin()   { return &*buffer_.begin(); }
    const char* begin() const  { return &*buffer_.begin(); }

    void makespace(size_t len)
    {
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |----------------------------|         |------------------|
/// 0                    <=  readerIndex <= writerIndex       <=size

/// |-------------------|-------------------------------------------|
///                kCheapPrepend                                   len

        // 真正可写的内存大小 + read区域前面空闲出来的大小 < 需要写的大小 + kCheapPrepend
        if(writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);//直接扩容
        }
        else    //把还未读的数据移动到前面
        {
/// | prependable bytes |  readable bytes    |  writable bytes  |
/// |----------------------------| readable  |------------------|
/// 0                    <=  readerIndex <= writerIndex    <=size
/// |-------------------| readable  |---------------------------|
/// 0         <=  readerIndex <= writerIndex               <=size
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                    begin() + writerIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;

        }


    }
    
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

