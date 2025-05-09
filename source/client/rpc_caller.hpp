#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include "requestor.hpp"
#include <future>
/*
        该模块核心：
            只需要向外提供几个rpc调用的接口，内部实现向服务端发送请求，等待获取结果即可
        理解：
        1、该层的设计属于requestor.hpp的上层。
        2、这里必须注意区分Rpc_Caller.hpp里面的回调和异步都是针对  “响应消息里面的result”   而言的，
        requestor.hpp里面的回调和异步都是针对于  “消息”  而言的，不是result结果，所以rpc_caller.hpp里面需要包装一下
*/
namespace zrcrpc
{
    namespace client
    {
        class RpcCaller
        {
        public:
            using Ptr = std::shared_ptr<RpcCaller>;
            using JsonAsynResponse = std::future<Json::Value>; // 针对Json::Value类型的result结构而言的future
            using JsonCallBackResponse = std::function<void(const Json::Value &)>;

            RpcCaller(const Reuqestor::Ptr &requestor) : _requestor(requestor)
            {
            }

        public:
            // 同步
            // 这里注意const和非const，有时候函数需要的const，但是传入的是非const，所以产生参数不匹配的情况
            bool call(const BaseConnection::Ptr &conn, const std::string &method,
                      const Json::Value &params, Json::Value &result)
            {
                // 1、根据传入的消息组织请求
                zrcrpc::RpcRequest::Ptr req = MessageFactory::create<RpcRequest>();
                if (!req)
                {
                    ELOG("请求创建失败");
                    return false;
                }
                req->setId(UUID::uuid());
                req->setMessageType(MType::REQ_RPC);
                req->setMethod(method);
                req->setParams(params);
                // 2、发送请求
                BaseMessage::Ptr resp;
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req), resp);
                if (!ret)
                {
                    ELOG("响应失败");
                    return false;
                }

                // 3、等待响应
                auto resp_msg = std::dynamic_pointer_cast<RpcResponse>(resp);
                if (resp_msg == nullptr)
                {
                    ELOG("向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("响应码错误,%s", ErrReason(resp_msg->responseCode()).c_str());
                    return false;
                }
                result = resp_msg->result();
                return true;
            }

            // 异步
            // 向服务器发送异步回调请求，设置回调函数，回调函数中会传入一个promise对象，在回调函数中去对promise设置数据
            /*
                这里的设计根据用户传递进来的参数，来实现异步的调用。
                前面的设计思路和同步是一样的，后面发送请求，就是创建promise指针对应里面保存结果Json::Value类型
            */
            bool call(const BaseConnection::Ptr &conn, const std::string &method,
                      const Json::Value &params, JsonAsynResponse &result)
            {
                // 1、根据传入的消息组织请求
                zrcrpc::RpcRequest::Ptr req = MessageFactory::create<RpcRequest>();
                if (!req)
                {
                    ELOG("请求创建失败");
                    return false;
                }
                req->setId(UUID::uuid());
                req->setMessageType(MType::REQ_RPC);
                req->setMethod(method);
                req->setParams(params);

                // 2、发送请求
                // 这里必须使用指针，不然这个promise是局部变量，函数结束后会被释放掉
                // 这里面的promise里面必须是Json::Value对象

                // 这里就是绑定这里的CallBack_callback函数，然后里面调用异步的send函数，然后阻塞在future.get()
                // 当收到response报文的时候，这里的rsp就会被设置，然后onResponse里面会调用这个回调函数CallBack_Promise，这里面的promise会设置Json::Value类型的result;
                auto resp_promise = std::make_shared<std::promise<Json::Value>>();
                auto cb = std::bind(&RpcCaller::CallBack_Promise, this, resp_promise, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req), cb);
                if (!ret)
                {
                    ELOG("异步Rpc请求失败!");
                    return false;
                }
                // 3、等待响应
                result = resp_promise->get_future();
                return true;
            }

            // 回调函数
            bool call(const BaseConnection::Ptr &conn, const std::string &method,
                      const Json::Value &params, JsonCallBackResponse &resp_cb)
            {
                // 1、根据传入的消息组织请求
                zrcrpc::RpcRequest::Ptr req = MessageFactory::create<RpcRequest>();
                if (!req)
                {
                    ELOG("请求创建失败");
                    return false;
                }
                req->setId(UUID::uuid());
                req->setMessageType(MType::REQ_RPC);
                req->setMethod(method);
                req->setParams(params);
                // 2、发送请求

                auto cb = std::bind(&RpcCaller::CallBack_callback, this, resp_cb, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req), cb);
                if (!ret)
                {
                    ELOG("回调Rpc请求失败!");
                    return false;
                }
                // 3、等待响应
                return true;
            }

        private:
            bool CallBack_Promise(std::shared_ptr<std::promise<Json::Value>> resp_promise, const BaseMessage::Ptr &resp)
            {

                // 这里的实际调用时机就是在requestor的onResponse里面，requestor里面的promise异步传入msg信息的时候，这里再次根据msg信息异步设置result对象
                auto resp_msg = std::dynamic_pointer_cast<RpcResponse>(resp);
                if (resp_msg == nullptr)
                {
                    ELOG("向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("响应码错误,%s", ErrReason(resp_msg->responseCode()));
                    return false;
                }
                resp_promise->set_value(resp_msg->result());
            }

            bool CallBack_callback(const JsonCallBackResponse &callback_resp, const BaseMessage::Ptr &resp)
            {
                auto resp_msg = std::dynamic_pointer_cast<RpcResponse>(resp);
                if (resp_msg == nullptr)
                {
                    ELOG("向下转换失败");
                    return false;
                }
                if (resp_msg->responseCode() != RCode::OK)
                {
                    ELOG("响应码错误,%s", ErrReason(resp_msg->responseCode()));
                    return false;
                }
                callback_resp(resp_msg->result());
            }

        private:
            Reuqestor::Ptr _requestor;
        };
    }
}