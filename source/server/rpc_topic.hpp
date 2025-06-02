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

        private:
            struct Subscriber
            {
            public:
                using Ptr = std::shared_ptr<Subscriber>;
                Subscriber(const BaseConnection::Ptr conn) : _conn(conn) {}
                void addTopic(const std::string &topic)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _topics.insert(topic);
                }
                void removeTopic(const std::string &topic)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _topics.erase(topic);
                }

            public:
                std::mutex _mutex;
                BaseConnection::Ptr _conn;               // 每个订阅者维护一个自己对应的连接
                std::unordered_set<std::string> _topics; // 每个订阅者自己所订阅主题全部都管理起来
            };

            struct Topic
            {
            public:
                using Ptr = std::shared_ptr<Topic>;
                Topic(const std::string &name) : _topic_name(name) {}
                void addSubscriber(const Subscriber::Ptr &subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _subscribers.insert(subscriber);
                }
                void removeSubscriber(const Subscriber::Ptr &subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _subscribers.erase(subscriber);
                }
                void onPublish(const BaseMessage::Ptr &msg)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    for (auto &ptr : _subscribers)
                    {
                        ptr->_conn->send(msg);
                    }
                }

            public:
                std::mutex _mutex;
                std::string _topic_name;                          // 维护一个主题自己的名字
                std::unordered_set<Subscriber::Ptr> _subscribers; // 将订阅了这个主题的所有的订阅者全部管理起来
            };

        public:
            using Ptr = std::shared_ptr<PSManager>;
            PSManager() {}

        public:
            void onTopicRequest(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                // 实现主题的创建，删除，订阅主题，取消订阅主题，主题消息的发布
                bool ret = true;
                switch (msg->operationType())
                {
                case TopicOptype::TOPIC_CREATE:
                    topicCreate(conn, msg);
                    break;
                case TopicOptype::TOCPIC_REMOVE:
                    topicRemove(conn, msg);
                    break;
                case TopicOptype::TOPIC_SUBSCRIBE:
                    ret = topicSubscriber(conn, msg);
                    break;
                case TopicOptype::TOPIC_CANCEL:
                    topicCancelSubscriber(conn, msg);
                    break;
                case TopicOptype::TOPIC_PUBLISH:
                    ret = topicPublish(conn, msg);
                    break;

                default:
                    return errResponse(conn, msg, RCode::INVALID_OPTYPE);
                }
                ret == false ? errResponse(conn, msg, RCode::NOT_FOUND_TOPIC) : topicResponse(conn, msg);
            }
            void onConnShutDown(const BaseConnection::Ptr &conn)
            {
                // 1、判断是不是订阅者，如果是订阅者那么接下来继续处理，如果不是就直接返回
                // 2、找到订阅者连接，找到和订阅者有关系的主题全部保存起来
                // 3、遍历所有相关的主题，然后删除主题里面的订阅者
                // 4、删除_conns的连接
                std::vector<Topic::Ptr> topics;
                Subscriber::Ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it_conn = _conns.find(conn);
                    if (it_conn == _conns.end())
                        return;
                    subscriber = it_conn->second;
                    // 由于subscriber里面维护的string集合，这里遍历string集合
                    for (auto &topic_name : subscriber->_topics)
                    {
                        // 通过_topics找到topicname对应的Topic::Ptr
                        auto it_topic = _topics.find(topic_name);
                        if (it_topic == _topics.end())
                            continue;
                        topics.emplace_back(it_topic->second);
                    }
                    _conns.erase(conn);
                }
                for (auto &e : topics)
                {
                    e->removeSubscriber(subscriber);
                }
            }

        private:
            void errResponse(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg, const RCode &rcode)
            {
                // 直接返回错误的消息
                auto resp_msg = zrcrpc::MessageFactory::create<TopicResponse>();
                resp_msg->setId(msg->id());
                resp_msg->setMessageType(MType::RSP_TOPIC);
                resp_msg->setResponseCode(rcode);
                conn->send(resp_msg);
            }
            void topicResponse(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)
            {
                // 创建对应的响应消息
                auto resp_msg = zrcrpc::MessageFactory::create<TopicResponse>();
                resp_msg->setId(msg->id());
                resp_msg->setMessageType(MType::RSP_TOPIC);
                resp_msg->setResponseCode(RCode::OK);
                conn->send(resp_msg);
            }

            // 根据msg里面的信息创建一个主题
            void topicCreate(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                //_topics里面加入一个主题
                std::string topic_name = msg->key();
                Topic::Ptr topic = std::make_shared<Topic>(topic_name);
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(topic_name);
                    if (it != _topics.end())
                        return;
                    else
                    {
                        _topics[topic_name] = topic;
                        return;
                    }
                }
            }

            // 根据msg里面的信息去移除一个主题
            void topicRemove(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                // 1、首先找到一个topic_name对应的topic主题
                // 2、然后根据先根据topic找到每个订阅者，先删除订阅者里面的topic
                // 3、最后删除_topics里面的连接
                Topic::Ptr topic;
                std::unordered_set<Subscriber::Ptr> subscribers;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it_topic = _topics.find(msg->key());
                    if (it_topic == _topics.end())
                    {
                        // 这里直接就是主题都没找到，直接返回成功就好
                        return;
                    }
                    topic = it_topic->second; // 找到主题连接

                    // 这里将topic里面的订阅者集合取出来，在锁外面去删除
                    subscribers = topic->_subscribers;
                    _topics.erase(it_topic);
                }
                for (auto &sub : subscribers)
                {
                    sub->removeTopic(topic->_topic_name);
                }
            }

            // 根据msg里面的信息对目标主题，进行订阅，订阅连接就是conn
            bool topicSubscriber(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                // 1、 先判断主题是否存在，如果不存在就返回false，创建主题以后增加订阅
                // 2、根据连接找到对应的订阅者，不存在就创建订阅者，订阅者增加主题
                // 3、修改_topics和_conns
                Topic::Ptr topic;
                Subscriber::Ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it_topic = _topics.find(msg->key());
                    if (it_topic == _topics.end())
                    {
                        // 主题都没有，直接返回false
                        return false;
                    }

                    topic = it_topic->second;
                    // 根据连接找到对应的订阅者，一般来说，找不到是正常的
                    auto it_sub = _conns.find(conn);
                    if (it_sub != _conns.end())
                    {
                        subscriber = it_sub->second;
                    }
                    else // 找不到就创建一个订阅者，修改_conns
                    {
                        // 创建订阅者
                        subscriber = std::make_shared<Subscriber>(conn);
                        _conns[conn] = subscriber;
                    }
                }

                // 主题增加订阅者
                // 这里如果不是新建的订阅者，在该函数中也会判断是否存在的，存在就插入失败
                topic->addSubscriber(subscriber);

                // 订阅者里面添加主题
                subscriber->addTopic(topic->_topic_name);
                return true;
            }

            // 根据msg里面的信息对目标主题，进行取消订阅
            void topicCancelSubscriber(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                // 1、首先找到主题
                // 2、找到主题对应的连接，先删除订阅者里面的主题
                // 3、最后删除主题里面的连接
                Topic::Ptr topic;
                Subscriber::Ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);

                    // 找到主题
                    auto it_topic = _topics.find(msg->key());
                    if (it_topic != _topics.end())
                        topic = it_topic->second;
                    else
                        return;

                    // 找到连接
                    auto it_sub = _conns.find(conn);
                    if (it_sub != _conns.end())
                        subscriber = it_sub->second;
                    else
                        return;
                    // _conns.erase(conn);
                }
                if (subscriber)
                    subscriber->removeTopic(topic->_topic_name);

                if (topic && subscriber)
                    topic->removeSubscriber(subscriber);
            }

            // 根据msg里面的信息对目标主题进行消息发布
            bool topicPublish(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg)
            {
                Topic::Ptr topic;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 找到主题
                    auto it_topic = _topics.find(msg->key());
                    if (it_topic == _topics.end())
                        return false;
                    topic = it_topic->second;
                }
                topic->onPublish(msg);
                return true;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::Ptr> _topics; // 主题名字--主题对应的类
            // 这里的_conns是提供的onshutdown函数使用的，一般只有断开连接的时候才使用这个函数去删除
            // 之间是Topic里面存储的是BaseConnection::Ptr连接，所以会用到_conns，现在里面是Subscriber就不需要使用_conns
            std::unordered_map<BaseConnection::Ptr, Subscriber::Ptr> _conns; // 连接 --- 订阅者（里面包含连接）
        };
    }
}
