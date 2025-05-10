#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <unordered_set>

/*
    该模块实现的是主题的中转服务器：对主题的管理：创建，删除，订阅主题，取消订阅主题，主题消息的发布

    服务端的设计：
        1、提供一个消息处理的回调函数给dispatcher模块
        2、存在一个消息的断开连接的处理函数
        3、存在两个哈希：
        <主题，对应的订阅者集合> ：针对每一个主题，将订阅该主题的订阅者管理起来，方便后续主题的推送和订阅者断开连接时候的删除
        <连接，和对应的订阅者>   ：方便连接断开的时候找到对应的订阅者
*/

namespace zrcrpc
{
    namespace server
    {
        /* PSManager = PublishSubsribeManager */
        class PSManager
        {
        public:
            using Ptr = std::shared_ptr<PSManager>;
            PSManager(){}
        public:
            struct Topic
            {
            public:
                using Ptr = std::shared_ptr<Topic>;
                Topic(){}
                void addSubscriber(const BaseConnection::Ptr &subscriber);
                void removeSubscriber(const BaseConnection::Ptr &subscriber);
                void onPublish(const BaseMessage::Ptr & msg);

            public:
                std::string _topic_name;                             // 维护一个主题自己的名字
                std::unordered_set<BaseConnection::Ptr> _subscribes; // 将订阅了这个主题的所有的订阅者全部管理起来
            };

            struct Subsrciber
            {
            public:
                using Ptr = std::shared_ptr<Subsrciber>;
                Subsrciber(){}
                void addTopic(const Topic::Ptr &topic);
                void removeTopic(const Topic::Ptr &topic);

            public:
                BaseConnection::Ptr _conn;       // 每个订阅者维护一个自己对应的连接
                std::vector<std::string> _topics; // 每个订阅者自己所订阅主题全部都管理起来
            };

        public:
            void onTopicRequest(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg);
            void onConnShutDown(const BaseConnection::Ptr &conn);

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::Ptr> _subscribers;
            std::unordered_map<BaseConnection::Ptr, Subsrciber::Ptr> _conns;
        };
    }
}
