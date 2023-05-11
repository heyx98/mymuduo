#include "Timestamp.h"
#include <time.h>

Timestamp::Timestamp():microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {}

Timestamp Timestamp::now()
{
    return Timestamp(time(NULL));
}

std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    /*
    C 库函数 int snprintf(char *str, size_t size, const char *format, ...) 设将可变参数(...)按照 
    format 格式化成字符串，并将字符串复制到 str 中，size 为要写入的字符的最大数目，超过 size 会被截断
    ，最多写入 size-1 个字符。
    */
    snprintf(buf, 128,"%4d/%02d/%02d %02d:%02d:%02d",
        tm_time->tm_year + 1900,
        tm_time->tm_mon + 1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec);
    return buf;

}
/*
#include <iostream>
int main()
{
    std::cout << Timestamp::now().toString() << std::endl;
    return 0;
}
*/

