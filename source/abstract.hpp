#pragma once
#include "fields.hpp"
#include <memory>
#include <functional>


namespace zrcrpc
{
    class BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>;
        virtual ~BaseMessage() {};
        virtual std::string Id() { return _rid; };
        virtual void SetId(const std::string id) { _rid = id; };
        virtual MType Mtype() { return _mtype; };
        virtual void SetMtype(MType &type) { _mtype = type; };
        virtual std::string Serialize() = 0;
        virtual bool Deserialize(const std::string &msg) = 0;
        virtual bool check() = 0;

    private:
        MType _mtype;
        std::string _rid;
    };

    class BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;
        virtual ~BaseBuffer() {};
        virtual size_t readableBytes() = 0;                   // 返回缓冲区里面的字节数
        virtual int32_t peekInt32() = 0;                      // 尝试取出4字节数据,不删除
        virtual bool retrieveInt32() = 0;                     // 删除四个字节数据
        virtual int32_t readInt32() = 0;                      // 读取4字节数据，并且删除掉
        virtual std::string retrieveAsString(size_t len) = 0; // 取出指定长度的字符串
    private:
    };
    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
        virtual ~BaseConnection() {};
        virtual void send(const BaseMessage::ptr &msg) = 0;
        virtual void shutdown() = 0;
        virtual bool connected() = 0;

    private:
    };

    class BaseProtocol
    {
    public:
        virtual ~BaseProtocol() {};

        virtual bool canProcessed(const BaseBuffer::ptr &buff) = 0;               // 看缓冲区是否存在数据
        virtual bool onMessage(BaseBuffer::ptr &buff, BaseMessage::ptr &msg) = 0; // 从缓冲区中取出数据
        virtual std::string serialize(const BaseMessage::ptr &msg) = 0;           // 如果存在数据就开始序列化

    private:
    };
    using ConnectionCallBack = std::function<void(BaseConnection::ptr &)>; // 连接建立成功时候的回调函数
    using CloseCallBack = std::function<void(BaseConnection::ptr &)>;
    using MessageCallBack = std::function<void(BaseConnection::ptr &, BaseBuffer::ptr &)>; // 暂时不知道干啥的???
    class BaseServer
    {
    public:
        using ptr = std::shared_ptr<BaseServer>;
        virtual ~BaseServer() {};
        virtual void setConnectionCallBack(const ConnectionCallBack &cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallBack(const CloseCallBack &cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallBack(const MessageCallBack &cb)
        {
            _cb_message = cb;
        }
        virtual void start() = 0;

    private:
        // 内部设置回调函数变量
        ConnectionCallBack _cb_connection;
        CloseCallBack _cb_close;
        MessageCallBack _cb_message;
    };

    class BaseClient
    {

    public:
        using ptr = std::shared_ptr<BaseClient>;
        virtual ~BaseClient() {};
        virtual void setConnectionCallBack(const ConnectionCallBack &cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallBack(const CloseCallBack &cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallBack(const MessageCallBack &cb)
        {
            _cb_message = cb;
        }
        virtual void connect() = 0;
        virtual void send(const BaseMessage::ptr &msg) = 0;
        virtual void shutdow() = 0;
        virtual bool connected() = 0;
        virtual BaseConnection::ptr connection() = 0;

    private:
        ConnectionCallBack _cb_connection;
        CloseCallBack _cb_close;
        MessageCallBack _cb_message;
    };

}
