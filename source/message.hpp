#pragma once

#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"

namespace zrcrpc
{
    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<JsonMessage>;
        virtual std::string Serialize() override
        {
            std::string body;
            bool ret = JSON::serialize(_body, body);
            if (ret == false)
            {
                ELOG("JsonMessage serialize failed");
                return "err";
            }
            return body;
        }
        virtual bool Deserialize(const std::string &msg) override
        {
            return JSON::deserialize(msg, _body);
            if (ret == false)
        }

    private:
        Json::Value _body;
    };

    class JsonRequest:public JsonMessage
    {
        public:
        using ptr=std::shared_ptr<JsonRequest>;
        private:
    };
    class RpcRequest:public JsonRequest
    {
        public:
        using ptr=std::shared_ptr<RpcRequest>;
        
        private:
    }
    class JsonResponse:public JsonMessage
    {
        public:
        using ptr=std::shared_ptr<JsonResponse>;
        private:
    };

}