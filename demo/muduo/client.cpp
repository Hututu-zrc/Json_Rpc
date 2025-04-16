#include "muduo/net/TcpClient.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"
#include "muduo/base/CountDownLatch.h"
#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>

class Dict_Client
{

public:
    Dict_Client(std::string ip, int port)
        : _loop(_loopthreaed.startLoop()), _client(_loop, muduo::net::InetAddress(ip, port), "dict_client"), _cntlatch(1)
    {
        _client.setConnectionCallback([&](const muduo::net::TcpConnectionPtr &conn)
                                      { this->OnConnection(conn); });
        _client.setMessageCallback(std::bind(&Dict_Client::OnMessage, this,
                                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 一开始我以为是有个单独的start函数的，这里直接编写到构造函数里面了，其实也可以编写个start函数
        //  连接服务器,这里直接放在构造函数里面，此时是非阻塞，可能就在连接当中
        _client.connect(); // 非阻塞必须要配合CountDownLatch使用
        _cntlatch.wait();
    }

    // 发送消息的时候需要conn连接，但是这里属于自己编写的部分
    // 所以Dict_Server里面需要自己维护一个_conn变量来保存回调函数的传来的连接指针.
    bool Send(std::string &msg)
    {
        if (_conn->connected() == false)
        {
            std::cout << "连接已经断开\n";
            return false;
        }
        _conn->send(msg);
        return true;
    }

private:
    // 编写了回调函数OnConnection和Onmessage
    // muduo库内部调用回调函数的时候，会传递回来连接的conn指针，方便操作

    void OnConnection(const muduo::net::TcpConnectionPtr &conn) // 连接成功的时候使用的
    {
        if (conn->connected())
        {
            std::cout << "连接成功\n";
            // 此时连接成功，调用回调函数，知道连接成功就唤醒wait
            _cntlatch.countDown(); // 计数器--，唤醒条件变量
            _conn = conn;
        }
        else
        {
            std::cout << "连接断开\n";
            _conn.reset();
        }
    }

    // 断开连接的时候使用的
    void OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buff, muduo::Timestamp)
    {
        _cntlatch.wait();
        std::string ret = buff->retrieveAllAsString();
        std::cout << "ret: " << ret << std::endl;
    }

private:
    muduo::net::TcpConnectionPtr _conn; // 获取客户端的连接

    // 这里必须使用EventLoopThread函数，因为_client.connect()是非阻塞的
    // 非阻塞就导致loop的接收环节不能和住执行流在一起，这里要实现并发执行
    // 要么创建一个线程，单独去运行loop环节，这里muduo库里面实现了直接使用就好
    muduo::net::EventLoopThread _loopthreaed;
    muduo::net::EventLoop *_loop;
    muduo::CountDownLatch _cntlatch;
    muduo::net::TcpClient _client;
};

int main()
{
    Dict_Client client("127.0.0.1", 8888);
    while (1)
    {
        std::string msg;
        std::cin >> msg;
        if (msg == "exit")
            return 0;
        client.Send(msg);
    }
    return 0;
}