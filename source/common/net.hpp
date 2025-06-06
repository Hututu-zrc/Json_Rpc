#pragma once
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/CountDownLatch.h"
#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"
#include "message.hpp"
#include <mutex>
#include <unordered_map>

namespace zrcrpc
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*                     MuduoBuffer类，包装Muduo库的接口，主要包含对缓冲区的基本操作                         */
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using Ptr = std::shared_ptr<MuduoBuffer>;
        virtual ~MuduoBuffer() = default;
        MuduoBuffer(muduo::net::Buffer *buff) : _buff(buff)
        {
        }
        virtual size_t readableBytes() const override
        {
            return _buff->readableBytes();
        }
        virtual int32_t peekInt32() const override
        {
            // muduo库会进行字节序的转换，从缓冲区去取出4btye字节的整形，将网络字节序转换为主机字节序
            return _buff->peekInt32();
        }
        virtual void retrieveInt32() override
        {
            _buff->retrieveInt32();
        }
        virtual int32_t readInt32() override
        {
            return _buff->readInt32();
        }
        virtual std::string retrieveAsString(size_t length) override
        {
            return _buff->retrieveAsString(length);
        }

    private:
        muduo::net::Buffer *_buff;
    };

    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::Ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }

    private:
    };
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*                LVProtocol类，主要判断缓冲区消息是否完整，以及将消息打包为可发送的报文，或者将缓冲区数据解析到对应消息当中                  */
    class LVProtocol : public BaseProtocol
    {
    public:
        //|--package_len--|--MType--|--IdLen--|--Id--|--body--|
        virtual ~LVProtocol() noexcept = default;

        // 判断缓冲区里面的数据长度是否满足一次报文的长度
        virtual bool canProcess(const BaseBuffer::Ptr &buffer) const override
        {
            // 缓冲区可能没有数据，muduo里面的断言就会报错
            // std::cout<<<<std::endl;
            // DLOG("buffer->readableBytes():%d", (int)buffer->readableBytes());
            if (buffer->readableBytes() < _lenFieldsLength)
            {
                // DLOG("taget");
                return false;
            }
            // 看能否后取出四个字节长度的数据,如果可以则取出四个字节，
            // DLOG("before peekint32");
            int32_t len = buffer->peekInt32();
            // DLOG("after peekInt32");
            if (buffer->readableBytes() < len + _lenFieldsLength)
                return false;

            return true;
        }

        // 这里的消息采用lv格式，即length--value
        // 第一个参数length表示后续的长度属于这个报文
        //|--package_len--|--MType--|--IdLen--|--Id--|--body--|

        //这个函数就相当于反序列化，将缓冲区数据拿出来，根据缓冲区数据构造一个message
        virtual bool onMessage(const BaseBuffer::Ptr &buffer, BaseMessage::Ptr &msg) override
        {

            // 1、将网络里面的数据全部提取出来
            int32_t packageLen = buffer->readInt32();
            int32_t mtype = buffer->readInt32();
            int32_t idLen = buffer->readInt32();
            std::string id = buffer->retrieveAsString(idLen);
            size_t bodyLen = packageLen - _mtypeFieldsLength - _idFieldsLength - id.size();
            // 网络当中的数据变为字符串
            std::string body = buffer->retrieveAsString(bodyLen);

            // 2、根据提取出来的数据创建消息类型,然后将数据写入到message里面
            msg = zrcrpc::MessageFactory::create((zrcrpc::MType)mtype);
            if (!msg)
            {
                ELOG("Failed to create message.");
                return false;
            }

            // 3、根据缓冲区数据写入到创建消息的信息
            // 反序列化
            bool ret = msg->deserialize(body);
            if (!ret)
            {
                ELOG("LVProtocol parsing failed.");
                return false;
            }
            msg->setMessageType((zrcrpc::MType)mtype);
            msg->setId(id);

            return true;
        }
        // 将数据里面的Json::Value类型_body拿出来，加上前缀字段，组成一个完整的报文
        //|--package_len--|--MType--|--IdLen--|--Id--|--body--|
        virtual std::string serialize(const BaseMessage::Ptr &message) const override
        {
            std::string body = message->serialize();
            MType mtype = (MType)htonl((uint32_t)message->messageType());
            std::string id = message->id();
            int32_t idLen = htonl(id.size());
            int32_t h_totalLen = _mtypeFieldsLength + _idFieldsLength + id.size() + body.size();
            int32_t totalLen = htonl(h_totalLen);

            // 这里to_string是错误的，假设totalLen是123
            // 理论上写入应该是二进制形式的四个字节07 00 00 00(这里使用16进制表示4个字节)
            // 实际上写入的"123"
            // 应该写入四个字节的数据的
            std::string sendData;
            sendData.reserve(h_totalLen);
            sendData.append((char *)&totalLen, _lenFieldsLength);
            sendData.append((char *)&mtype, _mtypeFieldsLength);
            sendData.append((char *)&idLen, _idFieldsLength);

            sendData += id + body;

            return sendData;
        }

    private:
        static const size_t _lenFieldsLength = 4;
        static const size_t _mtypeFieldsLength = 4;
        static const size_t _idFieldsLength = 4;
    };

    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::Ptr create(Args... args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }

    private:
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*                   MuduoConnection类，将Muduo库的TcpConnectionPtr进行封装
                        将传递进来的消息调用Protocol的接口打包后发送；关闭连接；判断连接是否正常
    */

    // 真正使用的时候，不需要创建多个MuduoConnection对象
    // 只需要传递进来参数就可以了
    class MuduoConnection : public BaseConnection
    {
    public:
        using Ptr = std::shared_ptr<MuduoConnection>;
        // 这里的connection要使用指针，不能只用TcpConnection，这个不需要拷贝
        MuduoConnection(const muduo::net::TcpConnectionPtr &con, const BaseProtocol::Ptr &protocol)
            : _con(con),
              _protocol(protocol)
        {
        }
        virtual ~MuduoConnection() noexcept = default;
        virtual void send(const BaseMessage::Ptr &message) override
        {
            // 这个是发送函数，，将传入的消息进行序列化后，再发送数据
            std::string msg = _protocol->serialize(message);
            _con->send(msg);
        }
        virtual void shutdown() override
        {
            _con->shutdown();
        }
        virtual bool isConnected() const override
        {
            return _con->connected();
        }

    private:
        muduo::net::TcpConnectionPtr _con;
        BaseProtocol::Ptr _protocol;
    };

    class ConnectionFactory
    {
    public:
        template <typename... Args>
        static BaseConnection::Ptr create(Args... args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }

    private:
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*                MuduoServer类， 实现三个函数start,onConnection,onMessage函数
                        onConnection实现连接断开和连接时候使用
                        onMessage消息到来的时候使用
    */

    class MuduoServer : public BaseServer
    {
    public:
        using Ptr = std::shared_ptr<MuduoServer>;
        virtual ~MuduoServer() = default;

        MuduoServer(int port)
            : _server(&_loop, muduo::net::InetAddress("127.0.0.1", port), "MuduoServer", muduo::net::TcpServer::kReusePort),
              _protocol(ProtocolFactory::create())
        {
        }

        virtual void start() override
        {
            // 先设置回调函数到muduo库的回调函数当中
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));

            // 启动服务器，开始循环检测
            _server.start();
            _loop.loop();
        }

    private:
        // 这里分为两个回调函数，OnConnection是给muduo库的回调函数
        // MuduoServer结构体里面的connection_callback_,close_callback_,message_callback_是用户给MuduoServer的
        // 在OnConntion当中会调用用户传入进来的回调函数

        // 这个回调函数是建立/断开连接时候用的回调函数
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            // 这里因为底层使用的是muduo库，但是后续编写肯定是使用自己的BaseConnection类
            // 所以这里使用哈希映射，将muduo库的connection和自己编写的Connection连接起来
            if (conn->connected()) // 连接成功
            {
                // 1、创建自己的连接,将muduo库的connection和BaseConnection映射关系建立起来
                // 2、然后调用自己的连接回调函数
                BaseConnection::Ptr muduoConn = ConnectionFactory::create(conn, _protocol);
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    if (_cons.find(conn) != _cons.end())
                    {
                        ELOG("this bcon already exist in the _cons.");
                        return;
                    }
                    _cons[conn] = muduoConn;
                }
                if (connection_callback_)
                    connection_callback_(muduoConn);
            }
            else // 连接断开，这里不是连接失败
            {
                // 1、删除哈希映射关系
                // 2、调用关闭回调函数
                BaseConnection::Ptr muduoConn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    if (_cons.find(conn) == _cons.end()) // 找不到该连接
                    {
                        return;
                    }
                    muduoConn = _cons[conn];
                    _cons.erase(conn);
                }
                if (close_callback_)
                    close_callback_(muduoConn);
            }
        }

        // 这个函数是创建消息的回调函数,给muduo库使用的，就是用来创建消息
        // muduo库实现的是将从网络里面接收消息到缓冲区，这里的回调函数就是缓冲区进行处理
        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buff, muduo::Timestamp)
        {
            BaseBuffer::Ptr muduoBuff = BufferFactory::create(buff);
            BaseConnection::Ptr muduoConn;
            while (1)
            {

                // 1、判断缓冲区里面数据是否合法，能否提取
                bool canprocess = _protocol->canProcess(muduoBuff);
                if (canprocess == false) // 这里一般是数据量不足
                {
                    // 存在一种情况，有人疯狂向服务器发送垃圾数据，都是无效信息
                    // 这时候就要判断，超过长度的数直接返回
                    // ELOG("数据量不足");
                    if (muduoBuff->readableBytes() > MaxSize)
                    {
                        ELOG("Buffer is overflowed");
                        break; // 如果是break，会产生死循环，一直打断，一直判断
                    }
                    break;
                }

                // 2、将缓冲区里面的数据提取出来放在BaseMsg里面
                BaseMessage::Ptr muduoMsg;
                bool ret = _protocol->onMessage(muduoBuff, muduoMsg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("This data is err in the buffer");
                    return;
                }
                // 3、根据muduo库的连接查哈希映射找到自己的BaseConnnection连接

                if (_cons.find(conn) == _cons.end())
                {
                    ELOG("conn not exist");
                    return;
                }
                muduoConn = _cons[conn];
                // 上面从缓冲区提取出来数据，但是不添加报文信息
                // 下面继续调用用户传入的回调函数然后进行报头的处理
                if (message_callback_)
                    message_callback_(muduoConn, muduoMsg);
            }
        }

    private:
        muduo::net::TcpServer _server;
        muduo::net::EventLoop _loop;
        BaseProtocol::Ptr _protocol; // 创建自己的BaseConnection时候需要用到这个，要在构造函数里面初始化
        std::mutex _mutex;
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::Ptr> _cons; // 这里的_con属于共享资源，可能被并发访问所以要加锁
        static const size_t MaxSize = (1 << 16);
    };

    class ServerFactory
    {
    public:
        template <typename... Args>
        static MuduoServer::Ptr create(Args... args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }

    private:
    };

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*                BaseClient类，
     */
    class MuduoClient : public BaseClient
    {
    public:
        using Ptr = std::shared_ptr<MuduoClient>;
        MuduoClient(std::string ip, int port) // 忘记初始化_protocol
            : _protocol(ProtocolFactory::create()),
              _cntlatch(1),
              _loop(_loopThread.startLoop()),
              _client(_loop, muduo::net::InetAddress(ip, port), "MuduoClient")

        {
        }
        virtual ~MuduoClient() = default;

        virtual void connect() override
        {
            _client.setMessageCallback(std::bind(&MuduoClient::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _client.setConnectionCallback(std::bind(&MuduoClient::OnConnection, this, std::placeholders::_1));
            _client.connect(); // 由于非阻塞的原因,这里使用该函数的时候，必须使用条件变量去等待
            _cntlatch.wait();
            DLOG("connect completed");
        }
        virtual void send(const BaseMessage::Ptr &message) override
        {
            bool ret = isConnected();
            if (ret == false)
            {
                ILOG("disconnected");
                return;
            }
            _conn->send(message);
        }
        virtual void shutdown() override
        {
            // return _client.
            _client.disconnect();
        }
        virtual bool isConnected() const override
        {
            return _conn && _conn->isConnected();
        }

        virtual BaseConnection::Ptr connection() const override // 返回_conn连接
        {
            return _conn;
            // if (isConnected())
            // {
            //     return ConnectionFactory::create(_conn, ProtocolFactory::create());
            // }
        }

    private:
        void OnConnection(const muduo::net::TcpConnectionPtr &conn) // 连接的时候使用的
        {
            if (conn->connected()) // 连接成功
            {

                _cntlatch.countDown(); // 计数器--，唤醒条件变量
                _conn = ConnectionFactory::create(conn, _protocol);
            }
            else // 连接断开
            {
                DLOG("连接断开");
                _conn.reset();
            }
        }

        void OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buff, muduo::Timestamp)
        {
            BaseBuffer::Ptr muduoBuff = BufferFactory::create(buff);
            while (1)
            {

                // 1、判断缓冲区里面数据是否合法，能否提取
                bool canprocess = _protocol->canProcess(muduoBuff);
                if (canprocess == false)
                {
                    // 存在一种情况，有人疯狂向服务器发送垃圾数据，都是无效信息
                    // 这时候就要判断，超过长度的数直接返回
                    if (muduoBuff->readableBytes() > MaxSize)
                    {
                        ELOG("Buffer is overflowed");
                        return;
                    }
                    break;
                }

                // 2、将缓冲区里面的数据提取出来放在msg里面
                BaseMessage::Ptr muduoMsg;
                bool ret = _protocol->onMessage(muduoBuff, muduoMsg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("This data is err in the buffer");
                    return;
                }

                if (message_callback_)
                {
                    // DLOG("调用回调函数");
                    message_callback_(_conn, muduoMsg);
                }
            }
        }

    private:
        // 这里注意成员变量的顺序和初始化之间的顺序不能乱，要先初始化loopthread,再loop，在使用loop去初始化client
        BaseConnection::Ptr _conn;
        BaseProtocol::Ptr _protocol;
        muduo::CountDownLatch _cntlatch;
        muduo::net::EventLoopThread _loopThread;
        muduo::net::EventLoop *_loop;
        muduo::net::TcpClient _client;

        // BaseProtocol::Ptr _protocol; // 创建自己的BaseConnection时候需要用到这个，要在构造函数里面初始化
        // std::mutex _mutex;
        // std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::Ptr> _cons; // 这里的_con属于共享资源，可能被并发访问所以要加锁
        static const size_t MaxSize = (1 << 16);
    };
    class ClientFactory
    {
    public:
        template <typename... Args>
        static MuduoClient::Ptr create(Args... args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }

    private:
    };

} // namespace zrcrpc
