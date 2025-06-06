#pragma once

#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"
#include "../common/dispather.hpp"
#include "rpc_topic.hpp"

namespace zrcrpc
{
    namespace client
    {
        /*
            该模块分为三个部分：
            1、注册客户端，只负责注册。
            2、发现客户端，只负责发现服务
            3、Rpc客户端，基本功能是负责Rpc响应消息的发送，然后兼具发现客户端的功能

            这里需要区分服务端的代码里面也可能包含着client
            ***这里的Provider和Discoverer都是分开的，互不干涉***
        */

        class RegistryClient
        {
        public:
            using Ptr = std::shared_ptr<RegistryClient>;
            RegistryClient(const std::string &ip, const int &port)
                : _requestor(std::make_shared<zrcrpc::client::Reuqestor>()),
                  _provider(std::make_shared<zrcrpc::client::Provider>(_requestor)),
                  _dispatcher(DispatcherFactory::create())
            {

                // requestor模块提供给dispatcher模块的回调函数
                auto requestor_cb = std::bind(&zrcrpc::client::Reuqestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<ServiceResponse>(zrcrpc::MType::RSP_SERVICE, requestor_cb);

                // dispatcher模块提供给client的回调函数
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }
            bool registryService(const std::string &method, Address &host)
            {
                return _provider->registryService(_client->connection(), method, host);
            }

        private:
            /*
                首先创建requestor，用来根据不同的ID返回不同的响应
                然后创建provider参数，用来进行服务的注册，直接包装一遍接口，原来的conn变成现在的client连接
                最后创建dispatcher模块，将requestor回调函数给dispatcher里面
                最后将diapatcher的回调函数加入到client里面
             */

            // requestor的核心是用来初始化Provider,具体的发送功能在Provider里面
            Reuqestor::Ptr _requestor; // 根据响应消息的Id，返回对应的响应报文
            zrcrpc::client::Provider::Ptr _provider;
            Dispatcher::Ptr _dispatcher;
            BaseClient::Ptr _client;
        };

        class DiscoveryClient
        {
        public:
            /*
                这里相对于服务注册模块会向dispatcher模块里面多注册一个回调函数。
                因为Discovery模块里面包括了一个服务上线和下线的回调函数
            */
            using Ptr = std::shared_ptr<DiscoveryClient>;
            DiscoveryClient(const std::string ip, int port, const Discoverer::OfflineCallBack &cb)
                : _requestor(std::make_shared<zrcrpc::client::Reuqestor>()),
                  _dicoverer(std::make_shared<zrcrpc::client::Discoverer>(_requestor, cb)),
                  _dispatcher(DispatcherFactory::create())
            {
                // requestor模块提供给dispatcher模块的回调函数
                auto requestor_cb = std::bind(&zrcrpc::client::Reuqestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<ServiceResponse>(zrcrpc::MType::RSP_SERVICE, requestor_cb);

                // 这里需要多注册一个discover里面的回调函数给dispatcher模块，就是服务上线和下线函数
                auto service_cb = std::bind(&zrcrpc::client::Discoverer::onServiceRequest, _dicoverer.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<ServiceRequest>(zrcrpc::MType::REQ_SERVICE, service_cb);

                // dispatcher模块提供给client的回调函数
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }
            bool discoverService(const std::string &method, Address &host) // 发现服务函数直接当作接口
            {
                return _dicoverer->discoverService(_client->connection(), method, host);
            }
            void shutdown()
            {
                _client->shutdown();
            }

        private:
            /*
                和注册模块差不多
            */
            Reuqestor::Ptr _requestor; // 也是用来初始化Discoverer模块的
            zrcrpc::client::Discoverer::Ptr _dicoverer;
            Dispatcher::Ptr _dispatcher;
            BaseClient::Ptr _client;
        };

        class RpcClient
        {
        public:
            /*
                该模块实现主要是分两种，一种是不开启服务发现的功能，一种是开启服务发现的功能
                *** 很重要的就是，这里如果不开启服务发现，那么传入的就是服务提供者的地址，维护一个_rpc_client，里面传入服务提供者的地址，就是服务请求直接发送到服务提供者 ***
                *** 如果是开启服务发现，传入的就是注册中心的地址，每次就是发送请求到注册中心，然后注册中心选择服务器，返回可提供服务端的ip+port地址，然后创建
                一个client，并且根据地址维护起来实现长连接的方式，当服务下线的时候，就删除这个连接 ***

                2、开启服务发现功能：
                    (1)开启服务发现功能，那么就需要维护一个<主机，对应的客户端>这个哈希集合，提供对该哈希的增删查改接口
                    (2)这个模块最终实现还是rpc响应消息的发送，所以还是将rpc_caller里面的call函数包装一遍
                    (3)由于维护了_rpc_clients哈希，当服务下线的时候，需要删除掉这个哈希里面的映射关系，所以需要向外提供一个delClient的接口
            */
            using Ptr = std::shared_ptr<RegistryClient>;
            RpcClient(bool enableDiscovey, const std::string ip, int port)
                : _enableDiscvory(enableDiscovey),
                  _requestor(std::make_shared<zrcrpc::client::Reuqestor>()),
                  _caller(std::make_shared<zrcrpc::client::RpcCaller>(_requestor)),
                  _dispatcher(DispatcherFactory::create())
            {
                // 这里的rpc_client只能接收到rpc_rsp的响应消息
                auto requestor_cb = std::bind(&zrcrpc::client::Reuqestor::onResponse, _requestor.get(),
                                              std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<RpcResponse>(zrcrpc::MType::RSP_RPC, requestor_cb);

                // 如果开启服务发现功能，那么这里就直接创建服务发现客户端
                // 这里就会有多个可以提供服务的客户端  add->{ {"127.0.0.1" ：8888},{"127.0.0.1" : 8899}  }
                if (_enableDiscvory)
                {
                    auto del = std::bind(&RpcClient::delClient, this, std::placeholders::_1);
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port, del);
                }
                else // 如果不开启服务发现功能，那么这里就是正常的rpc客户端的响应
                {
                    // 客户端->add(10,20)->服务器 然后这里就是服务器向客户端返回计算结果的响应
                    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(),
                                                std::placeholders::_1, std::placeholders::_2);
                    _rpc_client = ClientFactory::create(ip, port);
                    _rpc_client->setMessageCallback(message_cb);
                    _rpc_client->connect();
                }
            }
            bool call(const std::string &method, const Json::Value &params, Json::Value &result)
            {
                // DLOG("进入到rpc_client的call");
                auto client = get_Method_Client(method);
                if (client.get() == nullptr)
                {
                    ELOG("获取客户端失败");
                    return false;
                }
                // DLOG("准备进入下一层caller");
                return _caller->call(client->connection(), method, params, result);
            }
            bool call(const std::string &method, const Json::Value &params, RpcCaller::JsonAsynResponse &result)
            {
                auto client = get_Method_Client(method);
                if (client.get() == nullptr)
                {
                    ELOG("获取客户端失败");
                    return false;
                }
                return _caller->call(client->connection(), method, params, result);
            }
            bool call(const std::string &method, const Json::Value &params, const RpcCaller::JsonCallBackResponse &resp_cb)
            {
                auto client = get_Method_Client(method);
                if (client.get() == nullptr)
                {
                    ELOG("获取客户端失败");
                    return false;
                }
                return _caller->call(client->connection(), method, params, resp_cb);
            }

        private:
            /*
                下面的增删查改都是为了开启服务发现功能而服务的，因为存在一个哈希
            */
            BaseClient::Ptr newClient(const Address &host)
            {
                // 根据对应的Address创建新的连接，给每个连接都添加对应的Dispatcher的回调函数，然后将新的连接添加到哈希表里面
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(),
                                            std::placeholders::_1, std::placeholders::_2);
                auto client = ClientFactory::create(host.first, host.second);
                client->setMessageCallback(message_cb);
                client->connect();

                // 添加到哈希表里面
                insertClient(host, client);
                return client;
            }
            BaseClient::Ptr getClient(const Address &host)
            {
                // DLOG("进入到getclient");
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _rpc_clients.find(host);
                    if (it == _rpc_clients.end())
                    {
                        // DLOG("在getclient里面创建的");
                        // 这里必须返回BaseClient::Ptr(),不能在这里直接newclient(host)
                        // 原因就是这里是加锁的状态，然后newclient里面存在一个insertclient，insertclient里面也会加锁，这里就会产生锁的竞争，导致程序死锁

                        return BaseClient::Ptr();
                    }
                    else
                        return it->second;
                }
            }
            BaseClient::Ptr get_Method_Client(const std::string &method)
            {
                if (_enableDiscvory)
                {
                    // DLOG("进入到get_Method_Client");
                    // 1. 通过服务发现，获取服务提供者地址信息
                    zrcrpc::Address host;
                    // 这里的host是输出型参数
                    auto ret = _discovery_client->discoverService(method, host);
                    if (ret == false)
                    {
                        ELOG("发现服务失败");
                        return BaseClient::Ptr();
                    }
                    // 2. 查看服务提供者是否已有实例化客户端，有则直接使用，没有则创建
                    {
                        auto client = getClient(host);
                        if (client.get() == nullptr)
                        {
                            // DLOG("进入到getmethodclient 此时client为空 开始创建");
                            return newClient(host);
                        }
                        return client;
                    }
                }
                else
                {
                    return _rpc_client;
                }
            }

            void insertClient(const Address &host, const BaseClient::Ptr &client)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients[host] = client;
            }
            void delClient(const Address &host)//这个函数是提供给discover_client里面的discoverer的下线函数使用的
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if (it == _rpc_clients.end())
                    return;
                _rpc_clients.erase(host);
            }
            struct AddressHash
            {
                size_t operator()(const Address &host) const
                {
                    // 使用 std::hash 计算各字段的哈希值
                    size_t h1 = std::hash<std::string>{}(host.first); // IP 地址
                    size_t h2 = std::hash<int>{}(host.second);        // 端口号

                    return h1 ^ (h2 << 1); // 简单组合
                }
            };
            void rpcClientShutdown()
            {
                _rpc_client->shutdown();
            }

        private:
            std::mutex _mutex; // 主要是保护_rcp_clients哈希
            bool _enableDiscvory;
            Reuqestor::Ptr _requestor;
            DiscoveryClient::Ptr _discovery_client;
            RpcCaller::Ptr _caller; // 用来进行rpc请求消息的发送
            Dispatcher::Ptr _dispatcher;
            BaseClient::Ptr _rpc_client;

            //这里的目的是为了维护一个长连接，将曾经请求的某个主机的地址和连接维护起来
            //只有当这边的服务提供方下线服务的的时候，才开始删除连接
            std::unordered_map<Address, BaseClient::Ptr, AddressHash> _rpc_clients;
        };

        class TopicClient
        {
        public:
            using Ptr = std::shared_ptr<TopicClient>;
            TopicClient(const std::string &ip, const int &port)
                : _requestor(std::make_shared<zrcrpc::client::Reuqestor>()),
                  _topic_manager(std::make_shared<zrcrpc::client::TopicManager>(_requestor)),
                  _dispatcher(DispatcherFactory::create())
            {

                // requestor模块提供给dispatcher模块的回调函数
                auto requestor_cb = std::bind(&zrcrpc::client::Reuqestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<TopicResponse>(zrcrpc::MType::RSP_TOPIC, requestor_cb);

                // topic_manager模块提供给dispatcher模块的回调函数
                auto topic_cb = std::bind(&TopicManager::onPublish, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<TopicRequest>(zrcrpc::MType::REQ_TOPIC, topic_cb);

                // dispatcher模块提供给client的回调函数
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }
            bool create(const std::string &key)
            {
                return _topic_manager->create(_client->connection(), key);
            }
            bool remove(const std::string &key)
            {
                return _topic_manager->remove(_client->connection(), key);
            }
            bool subscribe(const std::string &key, const TopicManager::SubscribeCallBack &cb)
            {
                return _topic_manager->subscribe(_client->connection(), key, cb);
            }
            bool cancelSubscribe(const std::string &key)
            {
                return _topic_manager->cancelSubscribe(_client->connection(), key);
            }
            bool publish(const std::string &key, const std::string &topic_msg)
            {
                return _topic_manager->publish(_client->connection(), key, topic_msg);
            }
            void shutdown()
            {
                _client->shutdown();
            }

        private:
            /*
                首先创建requestor，用来根据不同的ID返回不同的响应
                然后创建_topic_manager参数，用来进行服务的注册，直接包装一遍接口，原来的conn变成现在的client连接
                最后创建dispatcher模块，将requestor回调函数给dispatcher里面
                最后将diapatcher的回调函数加入到client里面
             */
            Reuqestor::Ptr _requestor; // 根据响应消息的Id，返回对应的响应报文
            zrcrpc::client::TopicManager::Ptr _topic_manager;
            Dispatcher::Ptr _dispatcher;
            BaseClient::Ptr _client;
        };
    }
}