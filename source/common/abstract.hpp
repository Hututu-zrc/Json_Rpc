/*
    这个类属于抽象层，规定后续使用的基类
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
        virtual std::string id() const { return id_; }
        virtual void setId(const std::string &id) { id_ = id; }
        virtual zrcrpc::MType messageType() const { return message_type_; }
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
        virtual size_t readableBytes() const = 0;
        virtual int32_t peekInt32() const = 0;
        virtual void retrieveInt32() = 0;
        virtual int32_t readInt32() = 0;
        virtual std::string retrieveAsString(size_t length) = 0;

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