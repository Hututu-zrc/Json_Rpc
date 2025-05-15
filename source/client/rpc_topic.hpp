#pragma once

#include "requestor.hpp"
#include <mutex>
#include <vector>
#include <unordered_map>

/*
    主题客户端主要分为两种主题发布客户端和主题订阅客户端，里面的功能实现的都是向中转服务端发送操作请求
    主题订阅的客户端订阅某个主题以后不可能一直等待，所以当主题发布客户端推送消息，中转服务器推送到订阅的客户端，接收到消息，调用dispatcher回调函数，里面再次调用函数去
    一、主题发布客户端：
    实现三个功能:1、主题创建 2、主题删除 3、消息发布
    二、主题订阅客户端：
    实现四个功能：1、主题创建 2、主题删除 3、主题订阅 4、主题取消订阅
    这里多一个实现回调函数给dispatcher，当中转服务器推送消息的时候，调用这个回调函数


    这里的TopicManager就是实现上述的主题功能，客户端再根据自己的需求来调用
*/
namespace zrcrpc
{
    namespace client
    {
        class TopicManager
        {
        public:
            using Ptr = std::shared_ptr<TopicManager>;
            using SubscribeCallBack = std::function<void(const std::string  &, const std::string &)>;
            TopicManager(const Reuqestor::Ptr &requestor) : _requestor(requestor) {}

            bool create(const BaseConnection::Ptr &conn, const std::string &key)
            {
                return createRequestMessage(conn, key, TopicOptype::TOPIC_CREATE);
            }
            bool remove(const BaseConnection::Ptr &conn, const std::string &key)
            {
                return createRequestMessage(conn, key, TopicOptype::TOCPIC_REMOVE);
            }
            bool subscribe(const BaseConnection::Ptr &conn, const std::string &key, const SubscribeCallBack &cb)
            {
                addSubscribeCallBack(key, cb);
                bool ret = createRequestMessage(conn, key, TopicOptype::TOPIC_SUBSCRIBE);
                if (ret == false) // 如果创建失败就移出对应的回调函数
                {
                    removeSubscribeCallBack(key);
                    return false;
                }
                return true;
            }
            bool cancelSubscribe(const BaseConnection::Ptr &conn, const std::string &key)
            {
                removeSubscribeCallBack(key);
                return createRequestMessage(conn, key, TopicOptype::TOPIC_CANCEL);
            }
            bool publish(const BaseConnection::Ptr &conn, const std::string &key, const std::string &topic_msg)
            {
                return createRequestMessage(conn, key, TopicOptype::TOPIC_PUBLISH,topic_msg);
            }
            void onPublish(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                // 首先判断消息类型是不是发布类型
                std::string topic_name = msg->key();
                if (msg->operationType() != TopicOptype::TOPIC_PUBLISH)
                {
                    ELOG("收到错误的消息类型")
                    return;
                }
                // 判断是否存在的对应的回调函数
                auto cb = getSubscribeCallBack(topic_name);
                if (!cb)
                {
                    ELOG("收到主题%s,不存在对应的回调函数", msg->key().c_str());
                    return;
                }
                cb( msg->key(),msg->message());
                return;
            }

        private:
            bool createRequestMessage(const BaseConnection::Ptr &conn, const std::string &key,
                                      const TopicOptype &otype, const std::string &topic_msg = std::string())
            {
                DLOG("进入到createRequestMessage");

                auto msg = MessageFactory::create<TopicRequest>();
                msg->setId(UUID::uuid());
                msg->setKey(key);
                msg->setMessageType(MType::REQ_TOPIC);
                msg->setOperationType(otype);
                if (otype == TopicOptype::TOPIC_PUBLISH)
                    msg->setMessage(topic_msg);

                BaseMessage::Ptr base_msg = MessageFactory::create<TopicResponse>();
                bool ret = _requestor->send(conn, msg, base_msg);

                DLOG("进入到createRequestMessage，消息发送完毕");

                if (ret == false)
                {
                    ELOG("主题创建失败");
                    return false;
                }
                auto resp_msg = std::dynamic_pointer_cast<TopicResponse>(base_msg);
                if (resp_msg.get() == nullptr)
                {
                    ELOG("主题消息向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("主题消息响应错误,%s", ErrReason(resp_msg->responseCode()).c_str());
                    return false;
                }
                return true;
            }
            //下面这几个函数都是给哈希使用的
            void addSubscribeCallBack(const std::string &key, const SubscribeCallBack &cb)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _callbacks.find(key);
                if (it == _callbacks.end())
                {
                    _callbacks[key] = cb;
                }
            }
            void removeSubscribeCallBack(const std::string &key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _callbacks.find(key);
                if (it != _callbacks.end())
                {
                    _callbacks.erase(key);
                }
            }
            SubscribeCallBack getSubscribeCallBack(const std::string &key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _callbacks.find(key);
                if (it != _callbacks.end())
                {
                    return _callbacks[key];
                }
                return SubscribeCallBack();
            }

        private:
            std::mutex _mutex;
            Reuqestor::Ptr _requestor;
            std::unordered_map<std::string, SubscribeCallBack> _callbacks; // 主题--回调函数
        };
    }

}