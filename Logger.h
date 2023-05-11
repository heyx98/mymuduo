#pragma once

#include "noncopyable.h"
#include <string>

//LOG_INFO("%s %d", arg1, arg2)
//__VA_ARGS__获取可变参的宏
//logmsgFormat：字符串，后面...是可变参
//为了防止造成意想不到的错误用do-while(0)

#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    } while(0)

#ifdef MUDEBUD
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif

//定义日志的级别  INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,   //普通信息
    ERROR,  //错误信息
    FATAL,  //core信息
    DEBUG,  //调试信息
};

//输出个日志类
class Logger : noncopyable
{
public:
    //获取唯一的日志实例对象
    static Logger& instance();
    //设置日志级别
    void setLogLevel(int level);
    //写日志
    void log(std::string msg);
    
private:
    int loglevel_;
    Logger() {}
    
};

