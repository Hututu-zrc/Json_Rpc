#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <jsoncpp/json/json.h>
namespace zrcrpc
{
    namespace server
    {
        /*
            1、这个hpp的核心就是接收客户端Rpc的请求，然后判断服务端是否可以提供该服务；
            如果不能提供就返回空的结果和错误的状态码；如果可以提供该服务，那么就返回结果和正确的状态的码；
            2、某个服务端对自己的服务的增删查改、创建新服务也是在这个hpp
        */

        enum class ParamType
        {
            BOOL = 0,
            INTEGRAL,
            NUMERIC, // 浮点型
            STRING,
            ARRAY,  // 数据
            OBJECT, // Json::Value对象

        };
        class ServiceDescribe
        {
            /*
                该类实现对服务描述的校验
                以及校验成功调用对应的回调函数
            */
        public:
            using Ptr = std::shared_ptr<ServiceDescribe>;
            using ServiceCallBack = std::function<void(const Json::Value &, Json::Value &)>;
            using ParamsDesc = std::pair<std::string, ParamType>; // 参数描述，比如"add"方法里面的参数就是"num1","num2" 都是对应INTEGRAL类型
            ServiceDescribe(std::string &&name, std::vector<ParamsDesc> &&paramdesc, ServiceCallBack &&cb, ParamType &&rtype)
                : _method_name(name), _param_desc(paramdesc), _call_back(cb), _rtype(rtype)
            {
            }

            std::string GetMethod()
            {
                return _method_name;
            }
            bool PraseParam(Json::Value params)
            {
                // 遍历该服务对应的参数容器，看看容器里面的元素在Json::Value对象当中是不是都存在
                for (auto &param : _param_desc)
                {
                    if (!params.isMember(param.first))
                    {
                        ELOG("参数%s缺失", param.first.c_str());
                        return false;
                    }
                    if (!Check(params[param.first], param.second))
                    {
                        ELOG("参数%s类型错误", param.first.c_str());
                        return false;
                    }
                }
                return true;
            }
            // 这里是将方法的参数传入，然后进行处理，最后返回结果
            bool Call(const Json::Value &params, Json::Value &result)
            {
                _call_back(params, result);
                if (CheckReturnValue(result) == false)
                {
                    ELOG("返回值参数类型错误");
                    return false;
                }
                return true;
            }

        private:
            bool CheckReturnValue(Json::Value val) const
            {
                return Check(val, _rtype);
            }
            bool Check(Json::Value val, ParamType ptype) const
            {
                switch (ptype)
                {
                case ParamType::BOOL:
                    return val.isBool();
                case ParamType::INTEGRAL:
                    return val.isIntegral();
                case ParamType::NUMERIC:
                    return val.isNumeric();
                case ParamType::STRING:
                    return val.isString();
                case ParamType::ARRAY:
                    return val.isArray();
                case ParamType::OBJECT:
                    return val.isObject();
                }
                return false;
            }

        private:
            std::string _method_name;
            std::vector<ParamsDesc> _param_desc;
            ServiceCallBack _call_back; // 实际的业务处理函数，比如"add"方法，那么第一个参数就是params参数，第二个参数就是result计算结果
            ParamType _rtype;           // 返回值类型
        };

        // 建造者模式，如果将接口都设置在ServiceDescribe里面，容易产生线程安全的问题
        // 这里的就是将参数设置功能单独分离开
        class SDFactory
        {
            /*
                该类实现构造出一个新的服务以及服务描述；
                创建一个新的服务和服务描述就通过该Factory类，然后使用build函数将Factory类里面的参数构造出ServiceDescribe类，并且返回指针，
                ServiceDescribe类里面专门负责参数校验以及校验完毕，调用对应的回调函数
            */
        public:
            using Ptr = std::shared_ptr<SDFactory>;

            void setServiceName(const std::string &name) { _name = name; }
            void setParamsDesc(const std::string &paramName, const ParamType &ptype) { _param_desc.emplace_back(paramName, ptype); }
            void setServiceServiceCallBack(const ServiceDescribe::ServiceCallBack &cb) { _call_back = cb; }
            void setRtype(const ParamType &rtype) { _rtype = rtype; }

            ServiceDescribe::Ptr build()
            {
                return std::make_shared<ServiceDescribe>(std::move(_name),
                                                         std::move(_param_desc),
                                                         std::move(_call_back),
                                                         std::move(_rtype));
            }

        private:
            std::string _name;
            std::vector<ServiceDescribe::ParamsDesc> _param_desc;
            ServiceDescribe::ServiceCallBack _call_back;
            ParamType _rtype; // 返回值类型
        };

        class ServiceManager // 这个类实现对服务的管理，增删查改
        {
        public:
            using Ptr = std::shared_ptr<ServiceManager>;
            void insert(ServiceDescribe::Ptr &sdptr)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services[sdptr->GetMethod()] = sdptr;
            }
            void del(ServiceDescribe::Ptr &sdptr)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _services.find(sdptr->GetMethod());
                if (it == _services.end())
                {
                    ELOG("删除的方法不存在");
                    return;
                }
                _services.erase(it);
            }
            ServiceDescribe::Ptr select(const std::string method)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _services.find(method);
                if (it == _services.end())
                {
                    ELOG("查找的方法不存在");
                    return std::shared_ptr<ServiceDescribe>();
                }
                return it->second;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, ServiceDescribe::Ptr> _services;
        };

        class Rpc_Router // 这个类实现服务的路由功能，调用上述的类
        {
        public:
            using Ptr = std::shared_ptr<Rpc_Router>;
            Rpc_Router() : _service_manager(std::make_shared<ServiceManager>()) {}

            // 这个函数以后注册到dispatcher模块里面的业务处理函数, **** 这里是rpc请求处理类型的消息***
            // 参数里面就是外部传递进来的rpc请求消息，然后该函数进行处理，返回rpcrespnose消息
            void onRequest(const zrcrpc::BaseConnection::Ptr &conn, const zrcrpc::RpcRequest::Ptr &request)
            {
                // 1. 查询客户端请求的方法描述--判断当前服务端能否提供对应的服务
                std::string method = request->method();
                ServiceDescribe::Ptr sdptr = _service_manager->select(method);
                if (sdptr.get() == nullptr)
                {
                    ELOG("%s 服务未找到！", request->method().c_str());
                    response(conn, request, Json::Value(), RCode::NOT_FOUND_SERVICE);
                    return;
                }
                // 2. 进行参数校验，确定能否提供服务
                bool canProvide = sdptr->PraseParam(request->params());
                if (canProvide == false)
                {
                    ELOG("%s 服务参数校验失败！", request->method().c_str());
                    response(conn, request, Json::Value(), RCode::INVALID_PARAMS);
                    return;
                }
                // 3. 调用业务回调接口进行业务处理
                Json::Value result;
                if (sdptr->Call(request->params(), result) == false)
                {
                    ELOG("%s 服务参数校验失败！", request->method().c_str());
                    response(conn, request, Json::Value(), RCode::INTERNAL_ERROR);
                    return;
                }
                // 4. 处理完毕得到结果，组织响应，向客户端发送

                // 这里很错误啊，要使用工厂，不要直接创建对象
                //  zrcrpc::RpcResponse::Ptr resp = std::make_shared<zrcrpc::RpcResponse>();
                response(conn, request, result, RCode::OK);
                return;
            }

            // 这里注册新方法的时候，需要插入很多信息，所以这里创建了SDFactory工厂类
            void registryMethod(ServiceDescribe::Ptr service)
            {
                _service_manager->insert(service);
            }

        private:
            void response(const BaseConnection::Ptr &conn,
                          const RpcRequest::Ptr &req,
                          const Json::Value &res, RCode rcode)
            {
                auto msg = MessageFactory::create<RpcResponse>();
                msg->setId(req->id()); // 这里就是为什么要传入req的原因
                msg->setMessageType(zrcrpc::MType::RSP_RPC);
                msg->setResponseCode(rcode);
                msg->setResult(res);
                conn->send(msg);
            }
            ServiceManager::Ptr _service_manager;
        };
    }
}