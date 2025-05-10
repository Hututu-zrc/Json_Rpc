#pragma once

#include "requestor.hpp"
#include <vector>
#include <unordered_map>

/*
    该模块主要实现的是服务注册、服务发现、服务上线/下线函数的编写

*/
namespace zrcrpc
{
    namespace client
    {
        class Provider
        {
            /*
                提供一个服务注册接口，该接口实现的就是向注册中心发送注册信息，进行服务注册
            */
        public:
            using Ptr = std::shared_ptr<Provider>;
            Provider(const Reuqestor::Ptr &requestor) : _requestor(requestor) {}
            bool registryService(const BaseConnection::Ptr &conn, const std::string &method, const Address &host)
            {
                // 构造servicerequest消息
                auto req_msg = MessageFactory::create<ServiceRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMessageType(MType::REQ_SERVICE);
                req_msg->setOperationType(ServiceOptype::SERVICE_REGISTRY);
                req_msg->setMethod(method);
                req_msg->setHost(host);

                // 使用requestor模块向注册中心发送请求
                BaseMessage::Ptr msg = MessageFactory::create<zrcrpc::ServiceResponse>();
                // 第二个参数是常量引用，能绑定到由std::shared_ptr<ServiceRequest>隐式转换产生的std::shared_ptr<BaseMessage>临时对象，所以类型兼容
                // 第三个参数是非常量引用，不能绑定到临时对象，因此std::shared_ptr<ServiceResponse>不能直接匹配std::shared_ptr<BaseMessage>&，类型不兼容。
                bool ret = _requestor->send(conn, req_msg, msg);
                if (ret == false)
                {
                    ELOG("服务请求发送失败");
                    return false;
                }
                auto resp_msg = std::dynamic_pointer_cast<ServiceResponse>(msg);
                if (resp_msg.get() == nullptr)
                {
                    ELOG("服务响应消息向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("服务响应错误,%s", ErrReason(resp_msg->responseCode()).c_str());
                    return false;
                }
                return true;
            }

        private:
            Reuqestor::Ptr _requestor;
            // std::unordered_map<std::string,std::vector<Address>> _method_hosts;
        };

        class Hosts
        {
            /*
                这个模块实现的就是将原本的vector<Address>进行包装，实现rr轮转的功能。
                (我对此抱有怀疑，如果一个服务提供者可以提供多种服务，那么有可能多个请求方，请求不同服务，但是请求到同一个主机上面)
            */
        public:
            using Ptr = std::shared_ptr<Hosts>;
            Hosts(const std::vector<Address> &hosts = std::vector<Address>()) : _hosts(hosts) {}
            Address chooseHost()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _hosts[(_index++ % _hosts.size())];
            }

            bool addHost(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _hosts.emplace_back(host);
                return true;
            }
            bool delHost(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _hosts.begin();
                while (it != _hosts.end())
                {
                    if (*it == host)
                    {
                        _hosts.erase(it);
                        return true;
                    }
                }
                ELOG("删除主机不存在");
                return false;
            }
            bool empty()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _hosts.empty();
            }

        private:
            std::mutex _mutex;
            std::size_t _index;
            std::vector<Address> _hosts;
        };

        class Discoverer
        {
            /*
                1、该模块实现的就是向注册中心进行服务发现，收到注册中心返回回来的可使用的主机集合，这里实现rr轮转的方式实现负载均衡
                2、提供dispather模块一个回调函数，实现服务的上线和下线的功能，实际就是添加或者删除_method_hosts里面的可用主机数量。
                因为，发现客户端里面维护了《服务，可调用主机》这个哈希，所以服务的上线和下线功能也必须在这里实现比较好
            */
        public:
            using Ptr = std::shared_ptr<Discoverer>;
            using OfflineCallBack = std::function<void(const Address &host)>;
            /*
                这个discoverService函数首先判断本主机是否已经存在服务主机集合，如果存在直接返回true
                如果不存在，那么就向注册中心发送消息，拿到注册中心记录的可以提供该服务的主机集合
            */

            Discoverer(Reuqestor::Ptr requestor, OfflineCallBack cb) : _requestor(requestor), _cb(cb) {}
            bool discoverService(const BaseConnection::Ptr &conn, const std::string &method, Address &host)
            {
                // 首先判断当前是否存在method对应的服务主机集合，如果存在那么就直接返回
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _method_hosts.find(method);
                    if (it != _method_hosts.end())
                    {
                        if (it->second->empty())
                            return false;
                        host = it->second->chooseHost();
                        return true;
                    }
                }

                // 服务主机集合不存在，这里向服务中心发送服务发现请求
                // 构造servicerequest消息
                auto req_msg = MessageFactory::create<ServiceRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMessageType(MType::REQ_SERVICE);
                req_msg->setOperationType(ServiceOptype::SERVICE_DISCOVERY);
                req_msg->setMethod(method);
                BaseMessage::Ptr send_msg = MessageFactory::create<ServiceResponse>();
                bool ret = _requestor->send(conn, req_msg, send_msg);
                if (ret == false)
                {
                    ELOG("服务请求发送失败");
                    return false;
                }
                auto resp_msg = std::dynamic_pointer_cast<ServiceResponse>(send_msg);
                if (resp_msg.get() == nullptr)
                {
                    ELOG("服务响应消息向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("服务响应错误,%s", ErrReason(resp_msg->responseCode()).c_str());
                    return false;
                }

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 走到这里证明收到注册中心返回的服务响应，创建服务主机集合，添加到_method_hosts里面，然后返回
                    auto hosts = std::make_shared<Hosts>(resp_msg->hosts());
                    if (hosts->empty()) // 判断注册中心返回的服务主机集合是否为空
                    {
                        ELOG("没有主机可以提供该服务%s", method.c_str());
                        return false;
                    }
                    _method_hosts[method] = hosts;
                    host = hosts->chooseHost();
                    return true;
                }
            }

            // 服务上线/下线通知  注册中心向发现者发送服务上线/下线的消息
            void onServiceRequest(const BaseConnection::Ptr &conn, const ServiceRequest::Ptr &msg)
            {
                auto otype = msg->operationType();
                std::string method = msg->method();
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    if (otype == ServiceOptype::SERVICE_ONLINE)
                    {
                        auto it = _method_hosts.find(method);
                        if (it == _method_hosts.end()) // 之前不存在可以提供该服务的主机，现在直接创建
                        {
                            Hosts::Ptr hosts = std::make_shared<Hosts>();
                            hosts->addHost(msg->host());
                            _method_hosts[method] = hosts;
                        }
                        else // 之前存在该主机
                        {
                            it->second->addHost(msg->host());
                        }
                    }
                    else if (otype == ServiceOptype::SERVICE_OFFLINE)
                    {
                        /*
                            服务下线的时候需要删除rpc_client里面的长连接信息
                        */
                        auto it = _method_hosts.find(method);
                        if (it == _method_hosts.end()) // 不存在可以提供该服务的主机
                        {
                            ELOG("提供该服务的主机不存在，无需删除");
                            return;
                        }
                        else
                        {
                            it->second->delHost(msg->host());
                            _cb(msg->host());
                        }
                    }
                    else
                    {
                        ELOG("服务操作类型错误");
                        return;
                    }
                }
            }

        private:
            std::mutex _mutex;
            OfflineCallBack _cb;
            Reuqestor::Ptr _requestor;
            std::unordered_map<std::string, Hosts::Ptr> _method_hosts;
        };
    }
}
