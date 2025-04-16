/*
    实现翻译服务器，客户端发送单词，服务器翻译
    目的：为了进一步熟悉muduo库的使用
    思路：实现必要的两个回调函数和start函数，已经设置必要的server变量和loop变量
    需要注意的是回调函数是类内函数，需要bind绑定指针才可以和muduo库里面配套使用

    我的初步了解：server变量初始化端口和ip地址，是否开启地址复用等等
    服务器的启动和多路转接开始等待是分开的，启动是server—>start() ,多路转接的循环等待是_loop->loop()

    翻译功能的实现应该单独使用一个函数的，这里就直接写在OnMessage函数里面了

*/

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"
#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>

class Dict_Server
{
public:
    Dict_Server(int port)
        : _server(&_loop, muduo::net::InetAddress("127.0.0.1", port), "dict_server", muduo::net::TcpServer::Option::kReusePort)
    {
        // 设置回调函数 //回调函数是类内函数，隐藏带参this指针
        _server.setConnectionCallback([&](const muduo::net::TcpConnectionPtr &conn)
                                      { this->OnConnection(conn); });
        _server.setMessageCallback(std::bind(&Dict_Server::OnMessage, this,
                                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void Start()
    {
        _server.start();
        _loop.loop();
    }

private:
    // 编写了回调函数OnConnection和Onmessage
    //muduo库内部调用回调函数的时候，会传递回来连接的conn指针，方便操作
    void OnConnection(const muduo::net::TcpConnectionPtr &conn) // 连接成功的时候使用的
    {
        if (conn->connected())
            std::cout << "连接成功\n";
        else
            std::cout << "连接断开\n";
    }

    // 断开连接的时候使用的
    void OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buff, muduo::Timestamp)
    {

        // buff是conn连接存储数据的缓冲区
        std::string msg = buff->retrieveAllAsString();
        std::unordered_map<std::string, std::string> dict_map = {
            {"Hello", "你好"},
            {"你好", "Hello"},
            {"World", "世界"},
            {"世界", "World"},
        };
        std::string ret;
        auto it = dict_map.find(msg);
        if (it == dict_map.end())
        {
            ret = "Unkonwn";
        }
        ret = it->second;
        conn->send(ret);
    }

private:
    muduo::net::EventLoop _loop;
    //server初始化的时候就会创建新的连接
    muduo::net::TcpServer _server;
};

int main()
{
    Dict_Server dict(8888);
    dict.Start();
    return 0;
}