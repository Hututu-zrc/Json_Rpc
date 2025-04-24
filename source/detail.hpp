/*
    1、实现日志宏的定义
    2、json的序列化和反序列化
    3、uuid的生成
*/
#pragma once
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <jsoncpp/json/json.h>
#include <atomic>
#include <iomanip>
#include <chrono>
#include <random>
namespace zrcrpc
{

// 日志调用的级别
#define LDBUG 0 // DEBUG
#define LINF 1  // INFO
#define LERR 2  // ERR
#define DEFAULT LDBUG

// 日志调用的宏，第一个参数是调用的级别，第二个是传入的打印的"%s %d"这种的，第三个参数是可变参数
#define LOG(level, format, ...)                                                            \
    {                                                                                      \
        if (level >= DEFAULT)                                                              \
        {                                                                                  \
            time_t t = time(NULL);                                                         \
            struct tm *lt = localtime(&t);                                                 \
            char timetmp[32];                                                              \
            strftime(timetmp, 31, "%m-%d %T", lt);                                         \
            printf("[%s][%s:%d]" format "\n", timetmp, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                  \
    }

// 下面是主要使用的日志打印主要方法，固定好了打印的级别
#define DLOG(format, ...) LOG(LDBUG, format, ##__VA_ARGS__)
#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class JSON
{
public:
    // 实现数据的序列化
    static bool serialize(const Json::Value &val, std::string &body)
    {
        std::stringstream ss;
        // 先实例化⼀个⼯⼚类对象
        Json::StreamWriterBuilder swb;
        // 通过⼯⼚类对象来⽣产派⽣类对象
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        int ret = sw->write(val, &ss);
        if (ret != 0)
        {
            ELOG("json serialize failed");
            return false;
        }
        body = ss.str();
        return true;
    }
    // 实现json字符串的反序列化
    static bool deserialize(const std::string &body, Json::Value &val)
    {
        // 实例化⼯⼚类对象
        Json::CharReaderBuilder crb;
        // ⽣产CharReader对象
        std::string errs;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val,
                                &errs);
        if (ret == false)
        {
            ELOG("json unserialize failed : %s", errs.c_str());
            return false;
        }
        return true;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UUID
{
public:
    static std::string uuid()
    {
        std::stringstream ss;
        // 1. 创建机器随机数对象
        std::random_device rd;
        // 2. 用rd生成的机器随机数为种子构造伪随机数
        std::mt19937 generator(rd());
        // 3.限定构造随机数的范围
        std::uniform_int_distribution<int> distribution(0, 255); // 保证8进制的随机数都是两位的范围
        // 4. 生成8个随机数，并且转换为八进制形式，每个八进制形式为2位
        for (int i = 0; i < 8; i++)
        {
            if (i == 4 || i == 6)
            {
                ss << '-';
            }
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
        }
        ss << '-';

        // 生成8字节序号 总共就是2*(8*8)-1的范围，冲突可能性很小
        //  8*8总共64bit位，每次取出一个字节出来加入字符串
        // 这里使用atomic库来保证字节的原子性递增
        std::atomic<size_t> seq(1);
        size_t cur = seq.fetch_add(1); // 这个方法会原子地将 seq 的当前值加上 1，并且返回 seq 的旧值
        for (int i = 7; i >= 0; i--)   // 从高位到地位的加入字符串中
        {
            if (i == 5)
                ss << '-';
            ss << std::setw(2) << std::setfill('0') << std::hex << (cur >> (i * 8) & 0xff);
        }

        return ss.str();
    }
};

}
