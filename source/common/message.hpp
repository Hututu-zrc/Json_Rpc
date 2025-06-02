/*
    定义JSON消息相关的基类和子类，以及消息工厂
*/

#pragma once
#include "detail.hpp"
#include "abstract.hpp"

namespace zrcrpc
{

    // BaseMessage里面只有id和messagetype类型
    // 这里的JsonMessage里面存在一个body类型，也就是消息传输的主体，
    // 然后向外提供序列化和反序列化的接口
    class JsonMessage : public BaseMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonMessage>;
        virtual ~JsonMessage() = default;

        // 将Json::Value类型的数据修改为字符串类型，方便传输
        std::string serialize() const override
        {
            std::string body;
            bool ret = JSON::serialize(body_, body);
            if (!ret)
            {
                ELOG("JsonMessage serialize failed");
                return "err";
            }
            return body;
        }

        // 这里的反序列化就是将原本的网络字符串解析为Json::Value格式
        bool deserialize(const std::string &message) override
        {
            return JSON::deserialize(message, body_);
        }

    protected:
        Json::Value body_;
    };

    class JsonRequest : public JsonMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonRequest>;

    protected:
    };

    class RpcRequest : public JsonRequest
    {
    public:
        /* 消息的body里面存在两个属性：method、parameters */
        using Ptr = std::shared_ptr<RpcRequest>;

        bool isValid() const override
        {
            if (body_[KEY_METHOD].isNull() || !body_[KEY_METHOD].isString())
            {
                ELOG("RPC request method is missing or not a string");
                return false;
            }
            if (body_[KEY_PARAMS].isNull() || !body_[KEY_PARAMS].isObject())
            {
                ELOG("RPC request params are missing or not an object");
                return false;
            }
            return true;
        }

        std::string method() const { return body_[KEY_METHOD].asString(); }
        void setMethod(const std::string &method) { body_[KEY_METHOD] = method; }
        Json::Value params() const { return body_[KEY_PARAMS]; }
        void setParams(const Json::Value &params) { body_[KEY_PARAMS] = params; }

    private:
    };

    class TopicRequest : public JsonRequest
    {
    public:
        /*消息的body里面存在两个属性：key、otype、msg (  msg属于是只有otype==TOPIC_PUBLISH  才会使用这个字段) */

        using Ptr = std::shared_ptr<TopicRequest>;

        bool isValid() const override
        {
            if (body_[KEY_TOPIC_KEY].isNull() || !body_[KEY_TOPIC_KEY].isString())
            {
                ELOG("Topic key is missing or not a string");
                return false;
            }
            if (body_[KEY_OPTYPE].isNull() || !body_[KEY_OPTYPE].isIntegral())
            {
                ELOG("Topic operation type is missing or not an integer");
                return false;
            }
            if (body_[KEY_OPTYPE].asInt() == static_cast<int>(TopicOptype::TOPIC_PUBLISH) &&
                (body_[KEY_TOPIC_MSG].isNull() || !body_[KEY_TOPIC_MSG].isString()))
            {
                ELOG("Topic message is missing or not a string");
                return false;
            }
            return true;
        }

        std::string key() const { return body_[KEY_TOPIC_KEY].asString(); }
        void setKey(const std::string &key) { body_[KEY_TOPIC_KEY] = key; }
        std::string message() const { return body_[KEY_TOPIC_MSG].asString(); }
        void setMessage(const std::string &message) { body_[KEY_TOPIC_MSG] = message; }
        TopicOptype operationType() const { return static_cast<TopicOptype>(body_[KEY_OPTYPE].asInt()); }
        void setOperationType(TopicOptype operation_type) { body_[KEY_OPTYPE] = static_cast<int>(operation_type); }

    private:
    };

    using Address = std::pair<std::string, int>; // ip-- port

    class ServiceRequest : public JsonRequest
    {
    public:
        using Ptr = std::shared_ptr<ServiceRequest>;
        /*
            消息的body里面存在两个属性：method、otype、host(服务发现的时候才会使用这个属性)
            向某个主机host传递method方法，执行otype操作
         */

        bool isValid() const override
        {
            if (body_[KEY_METHOD].isNull() || !body_[KEY_METHOD].isString())
            {
                ELOG("Service method is missing or not a string");
                return false;
            }
            if (body_[KEY_OPTYPE].isNull() || !body_[KEY_OPTYPE].isIntegral())
            {
                ELOG("Service operation type is missing or not an integer");
                return false;
            }
            if (body_[KEY_OPTYPE].asInt() != static_cast<int>(ServiceOptype::SERVICE_DISCOVERY) &&
                (body_[KEY_HOST].isNull() || !body_[KEY_HOST].isObject() ||
                 body_[KEY_HOST][KEY_HOST_IP].isNull() || !body_[KEY_HOST][KEY_HOST_IP].isString() ||
                 body_[KEY_HOST][KEY_HOST_PORT].isNull() || !body_[KEY_HOST][KEY_HOST_PORT].isIntegral()))
            {
                ELOG("Service host parameters are missing or invalid");
                return false;
            }
            return true;
        }

        std::string method() const { return body_[KEY_METHOD].asString(); }
        void setMethod(const std::string &method) { body_[KEY_METHOD] = method; }
        Address host() const { return {body_[KEY_HOST][KEY_HOST_IP].asString(), body_[KEY_HOST][KEY_HOST_PORT].asInt()}; }
        void setHost(const Address &host)
        {
            body_[KEY_HOST][KEY_HOST_IP] = host.first;
            body_[KEY_HOST][KEY_HOST_PORT] = host.second;
        }
        ServiceOptype operationType() const { return static_cast<ServiceOptype>(body_[KEY_OPTYPE].asInt()); }
        void setOperationType(ServiceOptype operation_type) { body_[KEY_OPTYPE] = static_cast<int>(operation_type); }

    private:
    };

    class JsonResponse : public JsonMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonResponse>;
        virtual ~JsonResponse() = default;

        bool isValid() const override
        {
            if (body_[KEY_RCODE].isNull() || !body_[KEY_RCODE].isIntegral())
            {
                ELOG("Response rcode is missing or not an integer");
                return false;
            }
            return true;
        }

        RCode responseCode() const { return static_cast<RCode>(body_[KEY_RCODE].asInt()); }
        void setResponseCode(RCode code) { body_[KEY_RCODE] = static_cast<int>(code); }

    protected:
    };

    class RpcResponse : public JsonResponse
    {
    public:

        /*
            消息的body里面存在两个属性：rcode、result 
        
        */

        using Ptr = std::shared_ptr<RpcResponse>;

        bool isValid() const override
        {
            if (body_[KEY_RCODE].isNull() || !body_[KEY_RCODE].isIntegral())
            {
                ELOG("RPC response rcode is missing or not an integer");
                return false;
            }
            if (body_[KEY_RESULT].isNull())
            {
                ELOG("RPC response result is missing");
                return false;
            }
            return true;
        }

        Json::Value result() const { return body_[KEY_RESULT]; }
        void setResult(const Json::Value &result) { body_[KEY_RESULT] = result; }

    private:
    };

    class TopicResponse : public JsonResponse
    {
    public:
        /*消息的body里面存在两个属性：rcode */

        using Ptr = std::shared_ptr<TopicResponse>;

    private:
    };

    class ServiceResponse : public JsonResponse
    {
    public:
        /*  消息的body里面存在两个属性：rcode、otype     收到执行结果rcode，otype代表执行操作的类型
            还存在method、hosts这两个属性，只有当otype==SERVICE_DISCOVERY
            这里的服务发现是：收到注册中心返回的方法和对应的可提供服务主机集合
        */

        using Ptr = std::shared_ptr<ServiceResponse>;

        bool isValid() const override
        {
            if (body_[KEY_RCODE].isNull() || !body_[KEY_RCODE].isIntegral())
            {
                ELOG("Service response rcode is missing or not an integer");
                return false;
            }
            if (body_[KEY_OPTYPE].isNull() || !body_[KEY_OPTYPE].isIntegral())
            {
                ELOG("Service operation type is missing or not an integer");
                return false;
            }
            if (body_[KEY_OPTYPE].asInt() == static_cast<int>(ServiceOptype::SERVICE_DISCOVERY) &&
                (body_[KEY_METHOD].isNull() || !body_[KEY_METHOD].isString() ||
                 body_[KEY_HOST].isNull() || !body_[KEY_HOST].isArray()))
            {
                ELOG("Service discovery response parameters are missing or invalid");
                return false;
            }
            return true;
        }

        ServiceOptype operationType() const { return static_cast<ServiceOptype>(body_[KEY_OPTYPE].asInt()); }
        void setOperationType(ServiceOptype operation_type) { body_[KEY_OPTYPE] = static_cast<int>(operation_type); }
        std::string method() const { return body_[KEY_METHOD].asString(); }
        void setMethod(const std::string &method) { body_[KEY_METHOD] = method; }
        std::vector<Address> hosts() const
        {
            std::vector<Address> addresses;
            for (const auto &host : body_[KEY_HOST])
            {
                addresses.emplace_back(host[KEY_HOST_IP].asString(), host[KEY_HOST_PORT].asInt());
            }
            return addresses;
        }
        void setHosts(const std::vector<Address> &addresses)
        {
            body_[KEY_HOST].clear();
            for (const auto &addr : addresses)
            {
                Json::Value val;
                val[KEY_HOST_IP] = addr.first;
                val[KEY_HOST_PORT] = addr.second;
                body_[KEY_HOST].append(val);
            }
        }

    private:
    };

    class MessageFactory
    {
    public:
        static BaseMessage::Ptr create(MType message_type)
        {
            switch (message_type)
            {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::Ptr();
        }

        template <typename T, typename... Args>
        static std::shared_ptr<T> create(Args &&...args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }

    private:
    };

} // namespace zrcrpc