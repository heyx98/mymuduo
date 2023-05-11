#include "Logger.h"
#include "Timestamp.h"
#include <iostream>

//获取唯一的日志实例对象
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

//设置日志级别
void Logger::setLogLevel(int level)
{
    loglevel_ = level;
}

//写日志
void Logger::log(std::string msg)
{
    switch (loglevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROE]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;    
    default:
        break;
    }

    //打印时间和msg
    std::cout << "time : " << Timestamp::now().toString() << " : " << msg << std::endl;

}