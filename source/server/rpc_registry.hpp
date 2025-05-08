#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <jsoncpp/json/json.h>
#include <unordered_set>
namespace zrcrpc
{
    namespace server
    {
        class ProviderManager // 一个节点主机，将自己所能提供的服务，在注册中心进行登记
        {
        public:
            using Ptr = std::shared_ptr<ProviderManager>;

            struct Provider
            {
            public:
                using Ptr = std::shared_ptr<Provider>;
                std::mutex _mutex;
                BaseConnection::Ptr _conn;         // 注册中心和服务提供者之间的连接
                Address _host;                     // ip--port 自己的主机地址
                std::vector<std::string> _methods; // 可以提供的方法的名字

            public:
                Provider(const BaseConnection::Ptr &c, const Address &host) : _conn(c), _host(host) {}
                void addMethod(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _methods.emplace_back(method);
                }
            };

        public:
            Provider::Ptr createProvider(const BaseConnection::Ptr &c, const Address &host, const std::string &method)
            {
                // 判断该连接是否存在，存在就不需要创建映射关系，直接在对应的方法下面添加提供者
                //  如果不存在，就创建一个provider，然后设置里面的信息，添加映射关系，然后添加方法对应的提供者
                Provider::Ptr provider;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if (it != _conns.end())
                    {
                        provider = it->second;
                    }
                    else
                    {
                        provider = std::make_shared<Provider>(c, host);
                        _conns[c] = provider;
                    }

                    _mprovides[method].emplace(provider);
                }

                provider->addMethod(method);
                return provider;
            }

            Provider::Ptr getProvider(const BaseConnection::Ptr &c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if (it != _conns.end())
                {
                    return it->second;
                }
                return Provider::Ptr();
            }

            void delProvider(const BaseConnection::Ptr &c)
            {
                // 找到对应的provider，删除连接，遍历该提供者可以提供的方法，删除哈希里面的映射关系
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if (it == _conns.end())
                {
                    return;
                }
                for (auto &method : it->second->_methods) // 遍历provider可以提供的所有方法
                {
                    _mprovides[method].erase(it->second); // 删除哈希里面对应方法的该提供者
                }

                // 删除连接
                _conns.erase(c);
            }

        private:
            std::mutex _mutex;
            // 这个是针对每个服务器，每个服务器都维护这个_methods这个哈希结构，每个服务器都知道谁可以提供什么服务
            //  某服务可以由哪些主机提供者提供，当有客户端访问服务中心的时候，告诉客户端谁可以提供服务
            std::unordered_map<std::string, std::unordered_set<Provider::Ptr>> _mprovides; // 服务--提供者集合

            // 这个是针对该服务器本身，连接该服务中心的conn连接对应相应的provider
            //  某连接和对应的provider，当连接断开的时候，找到对应的提供者，告知发现者，该提供者的所有服务全部下架
            std::unordered_map<zrcrpc::BaseConnection::Ptr, Provider::Ptr> _conns; // 连接--对应的提供者
        };

        class DiscovererManager // 服务发现其实就是询问注册中心，谁能为自己提供指定的服务，将节点信息给保存起来以待后用
        {
        public:
            using Ptr = std::shared_ptr<DiscovererManager>;

            struct Discoverer // 某个连接（也就是某个可以提供服务的提供者）可以提供哪些方法
            {
            public:
                // 服务的发现者属性： 需要的方法 && 客户端的连接
                using Ptr = std::shared_ptr<Discoverer>;
                std::mutex _mutex;

                BaseConnection::Ptr _conn;
                std::vector<std::string> _methods;

            public:
                Discoverer(const BaseConnection::Ptr &conn) : _conn(conn) {}
                void addMethod(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _methods.emplace_back(method);
                }
            };

        public:
            Discoverer::Ptr createDiscovery(const BaseConnection::Ptr &c, const std::string &method)
            {
                Discoverer::Ptr discoverer;
                {
                    std::unique_lock<std::mutex> lock();
                    auto it = _conns.find(c);
                    if (it != _conns.end())
                    {
                        discoverer = it->second;
                    }
                    else
                    {
                        discoverer = std::make_shared<Discoverer>(c);
                        _conns[c] = discoverer;
                    }
                    _mdiscoverers[method].emplace(discoverer);
                }
                discoverer->addMethod(method);
                return discoverer;
            }

            void delDiscovery(const BaseConnection::Ptr &c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if (it == _conns.end())
                {
                    return;
                }
                for (auto &method : it->second->_methods)
                {
                    _mdiscoverers[method].erase(it->second);
                }

                // 删除连接
                _conns.erase(c);
            }
            void onLineNotify(const std::string &method, const Address &host)
            {
                Notify(method, host, ServiceOptype::SERVICE_ONLINE);
                return;
            }
            void offLineNotify(const std::string &method, const Address &host)
            {
                Notify(method, host, ServiceOptype::SERVICE_OFFLINE);
                return;
            }

        private:
            void Notify(const std::string &method, const Address &host, const ServiceOptype otype)
            {
                // 根据方法找到对应的发现者，创建消息告诉发现者，某主机的某服务上线或者下线
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _mdiscoverers.find(method);
                if (it == _mdiscoverers.end())
                {
                    // 该服务没有发现者
                    return;
                }
                auto msg = zrcrpc::MessageFactory::create<ServiceRequest>();
                msg->setId(UUID::uuid());
                msg->setMessageType(MType::REQ_SERVICE);
                msg->setHost(host);
                msg->setMethod(method);
                msg->setOperationType(otype);

                for (auto &discoverer : _mdiscoverers[method])
                {
                    discoverer->_conn->send(msg);
                }
            }

        private:
            std::mutex _mutex;
            // 这个服务有哪些客户端在使用
            std::unordered_map<std::string, std::unordered_set<Discoverer::Ptr>> _mdiscoverers; // 服务--发现者集合
            // 这个连接对应的发现者是谁，当这个发现者断开连接，后续的服务上线和下线就不需要对发现者继续通知了
            std::unordered_map<zrcrpc::BaseConnection::Ptr, Discoverer::Ptr> _conns; // 连接--对应的发现者
        };

        class PDManager
        {
        public:
            using Ptr = std::shared_ptr<PDManager>;
            PDManager()
                : _providers(std::make_shared<ProviderManager>()), _dicoveries(std::make_shared<DiscovererManager>())
            {
            }
            void onServiceRequest(const BaseConnection::Ptr &conn, const ServiceRequest::Ptr &msg)
            {
                // 先拿到消息里面的服务操作类型
                auto otype = msg->operationType();
                if (otype == ServiceOptype::SERVICE_REGISTRY)
                {
                    // 服务提供者 1、提供服务注册 2、上线通知
                    _providers->createProvider(conn, msg->host(), msg->method());
                    _dicoveries->onLineNotify(msg->method(), msg->host());
                }
                else if (otype == ServiceOptype::SERVICE_DISCOVERY)
                {
                    // 服务发现者 2、提供发现的服务
                    _dicoveries->createDiscovery(conn, msg->method());
                }
                else
                {
                    ELOG("服务类型出现错误");
                }
            }
            // 这里是需要判断连接时提供者还是发现者
            void onConnShutDown(const BaseConnection::Ptr &conn)
            {
                // 找到连接对应的提供方
                auto provider = _providers->getProvider(conn);
                if (provider.get() != nullptr) // 指针不为空，证明这个连接时提供者的连接
                {
                    // 对提供方所提供的每个服务进行下线处理
                    for (auto &method : provider->_methods)
                    {
                        _dicoveries->offLineNotify(method, provider->_host);
                    }
                    // 删除该提供方的连接
                    _providers->delProvider(conn);
                }
                else // 发现者连接
                {
                    // 清理发现者连接
                    _dicoveries->delDiscovery(conn);
                }
            }

        private:
            ProviderManager::Ptr _providers;
            DiscovererManager::Ptr _dicoveries;
        };
    }
}