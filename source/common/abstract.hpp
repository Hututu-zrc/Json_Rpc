/*
    这个类属于抽象层，规定后续使用的基类
    该类规定了一些基本的属性和抽象接口，后续实现接口即可
*/

#pragma once
#include <memory>
#include <functional>
#include "fields.hpp"

namespace zrcrpc
{

    class BaseMessage
    {
    public:
        using Ptr = std::shared_ptr<BaseMessage>;
        virtual ~BaseMessage() = default;
        virtual zrcrpc::MType messageType() const { return message_type_; }
        virtual std::string id() const { return id_; }

        virtual void setId(const std::string &id) { id_ = id; }
        virtual void setMessageType(const zrcrpc::MType &type) { message_type_ = type; }

        virtual std::string serialize() const = 0;
        virtual bool deserialize(const std::string &message) = 0;
        virtual bool isValid() const = 0;

    protected:
        zrcrpc::MType message_type_;
        std::string id_;
    };

    class BaseBuffer
    {
    public:
        using Ptr = std::shared_ptr<BaseBuffer>;
        virtual ~BaseBuffer() = default;
        virtual size_t readableBytes() const = 0;                // 返回缓冲区的字节大小
        virtual int32_t peekInt32() const = 0;                   // 尝试在缓冲区中取出一个int32大小的字节，但是不删除缓冲区的数据
        virtual void retrieveInt32() = 0;                        // 删除缓冲区的一个int32大小
        virtual int32_t readInt32() = 0;                         // 该函数从缓冲区里面读取一个int32，并且删除一个int32，功能的peekint+retrieveint32
        virtual std::string retrieveAsString(size_t length) = 0; // 删除缓冲区的len大小的长度，并且按照字符串返回

    private:
    };

    class BaseConnection
    {
    public:
        using Ptr = std::shared_ptr<BaseConnection>;
        virtual ~BaseConnection() noexcept = default;
        virtual void send(const BaseMessage::Ptr &message) = 0;
        virtual void shutdown() = 0;
        virtual bool isConnected() const = 0;

    private:
    };

    class BaseProtocol
    {
    public:
        using Ptr = std::shared_ptr<BaseProtocol>;

        virtual ~BaseProtocol() = default;
        virtual bool canProcess(const BaseBuffer::Ptr &buffer) const = 0;
        virtual bool onMessage(const BaseBuffer::Ptr &buffer, BaseMessage::Ptr &message) = 0;
        virtual std::string serialize(const BaseMessage::Ptr &message) const = 0;

    private:
    };

    using ConnectionCallback = std::function<void(const BaseConnection::Ptr &)>;
    using CloseCallback = std::function<void(const BaseConnection::Ptr &)>;
    using MessageCallback = std::function<void(const BaseConnection::Ptr &, const BaseMessage::Ptr &)>;

    class BaseServer
    {
    public:
        using Ptr = std::shared_ptr<BaseServer>;
        virtual ~BaseServer() = default;
        // 三个回调函数，在适时的时候被使用
        virtual void setConnectionCallback(const ConnectionCallback &callback)
        {
            connection_callback_ = callback;
        }
        virtual void setCloseCallback(const CloseCallback &callback)
        {
            close_callback_ = callback;
        }
        virtual void setMessageCallback(const MessageCallback &callback)
        {
            message_callback_ = callback;
        }
        virtual void start() = 0;

    protected:
        ConnectionCallback connection_callback_;
        CloseCallback close_callback_;
        MessageCallback message_callback_;
    };

    class BaseClient
    {
    public:
        using Ptr = std::shared_ptr<BaseClient>;
        virtual ~BaseClient() = default;
        virtual void setConnectionCallback(const ConnectionCallback &callback)
        {
            connection_callback_ = callback;
        }
        virtual void setCloseCallback(const CloseCallback &callback)
        {
            close_callback_ = callback;
        }
        virtual void setMessageCallback(const MessageCallback &callback)
        {
            message_callback_ = callback;
        }
        virtual void connect() = 0;
        virtual void send(const BaseMessage::Ptr &message) = 0;
        virtual void shutdown() = 0;
        virtual bool isConnected() const = 0;
        virtual BaseConnection::Ptr connection() const = 0;

    protected:
        ConnectionCallback connection_callback_;
        CloseCallback close_callback_;
        MessageCallback message_callback_;
    };

} // namespace zrcrpc