#pragma once

#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();
    //explicit关键字 防止类构造函数的隐式自动转换
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    /* data */
    int64_t microSecondsSinceEpoch_;
};
