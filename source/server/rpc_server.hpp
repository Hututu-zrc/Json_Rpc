#pragma once

#include "../common//dispather.hpp"
#include "rpc_registry.hpp"
#include "rpc_router.hpp"
#include "../client/rpc_client.hpp"
namespace zrcrpc
{
    namespace server
    {
        class RegistryServer
        {
        public:
            /* 该注册服务端，核心就是维护PDManager，该服务端就是服务中心，用来管理提供者和发现者的消息*/
            using Ptr = std::shared_ptr<RegistryServer>();
            RegistryServer(int port)
                : _dispatcher(DispatcherFactory::create()),
                  _pdmanager(std::make_shared<PDManager>())
            {
                // 这里的message_cb是提供给server的回调函数
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(),
                                            std::placeholders::_1, std::placeholders::_2);

                // 这是_pdmanager提供给dispatcher类的服务请求回调函数
                auto manager_cb = std::bind(&zrcrpc::server::PDManager::onServiceRequest, _pdmanager.get(),
                                            std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<ServiceRequest>(zrcrpc::MType::REQ_SERVICE, manager_cb);

                // 这是RegistryServer提供给server类的关闭连接的响应函数
                auto close_cb = std::bind(&zrcrpc::server::RegistryServer::onConnShutDown, this,
                                          std::placeholders::_1);

                _server->setCloseCallback(close_cb);
                _server = ServerFactory::create(port);
                _server->setMessageCallback(message_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutDown(const BaseConnection::Ptr &conn)
            {
                _pdmanager->onConnShutDown(conn);
            }

        private:
            Dispatcher::Ptr _dispatcher;
            server::PDManager::Ptr _pdmanager;
            BaseServer::Ptr _server;
        };

        class RpcServer
        {
        public:
            /*
                该服务端基本功能是提供Rpc消息响应发送
            */
            using Ptr = std::shared_ptr<RpcServer>();
            // access_addr是给server的主机地址，reg_addr是给_reg_client的主机地址，告诉客户端，注册中心的地址是多少
            // access_addr如果是云服务器就需要主要，这里的ip必须公网ip地址，不能是内网地址
            RpcServer(const Address &access_addr, bool enableRegClient = false, const Address &reg_addr = Address())
                : _enableRegClient(enableRegClient),
                  _dispatcher(DispatcherFactory::create()),
                  _router(std::make_shared<Rpc_Router>()),
                  _access_addr(access_addr)
            {
                if (_enableRegClient) // 如果服务注册功能开启，那么这里就创建注册客户端
                {
                    _reg_client = std::make_shared<client::RegistryClient>(reg_addr.first, reg_addr.second);
                }
                // 这里的message_cb是提供给server的回调函数
                auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, _dispatcher.get(),
                                            std::placeholders::_1, std::placeholders::_2);

                // 这是Rpc_Router提供给dispatcher类的服务响应回调函数
                auto manager_cb = std::bind(&zrcrpc::server::Rpc_Router::onRequest, _router.get(),
                                            std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registryCallBack<RpcRequest>(zrcrpc::MType::REQ_RPC, manager_cb);

                _server = ServerFactory::create(access_addr.second);
                _server->setMessageCallback(message_cb);
            }
            void registryMethod(ServiceDescribe::Ptr service)
            {
                if (_enableRegClient) // 如果开启服务注册客户端，那么这里注册方法的时候，就要往客户端里面增加一份
                {
                    // 这里传入的地址是指的，我这个主机地址，向注册中心注册某种方法，所以这里传入的主机我的地址
                    _reg_client->registryService(service->GetMethod(), _access_addr);
                }
                _router->registryMethod(service);
            }

            void start()
            {
                _server->start();
            }

        private:
            bool _enableRegClient;
            Dispatcher::Ptr _dispatcher;
            Rpc_Router::Ptr _router;
            Address _access_addr;
            client::RegistryClient::Ptr _reg_client;
            BaseServer::Ptr _server;
        };
    }
}
