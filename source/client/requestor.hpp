#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <future>

/*
           该模块核心：
                这个模块属于客户端的公共模块，不止于处理RpcCaller，还有TopicCaller等等
                所以这里面没有写固定，只是对于BaseMessage的基本处理
                实现的其实是消息响应的中转中心，核心就是根据消息的id，合适时间返回响应报文
            主要功能：提供send接口发送到服务提供方；提供一个收到响应消息的回调函数

*/

namespace zrcrpc
{
    namespace client
    {

        class Reuqestor
        {
        public:
            /*
                功能：
                    1、根据消息的id，合适时间返回响应报文
                    2、向外提供两个发送的接口，然后客户端调用该接口，不同的send接口有不同的describe描述
                    这里主要是callback回调函数和异步处理函数，当收到响应消息的时候，就调用对应的方法
            */

            using Ptr = std::shared_ptr<Reuqestor>;
            // 这个回调函数是提供给rpc_caller模块的，主要是接收到响应消息的时候，获取响应消息的处理
            using RequestCallBack = std::function<void(const BaseMessage::Ptr &)>;
            using AsynResponse = std::future<BaseMessage::Ptr>; // 针对response消息而言的future

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 内部类
            //  这里就是为了解决多线程当中存在的请求响应产生的时序问题，如何判断返回的响应报文是不是自己想要的响应报文
            //  将原本的request请求进行封装
            struct RequestDescribe
            {
                /*
                    这里的成员变量里面的(_response + _rtype) 和 _cb是一个级别的，这个是取决于什么方式获得结果
                    核心还是request消息
                */

            public:
                using Ptr = std::shared_ptr<RequestDescribe>;

            public:
                BaseMessage::Ptr _request;
                std::promise<BaseMessage::Ptr> _response;
                RequestCallBack _cb;
                RpcType _rtype;//这里的rpc请求的类型
            };

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            // 当中转中心收到响应以后的处理函数，这个函数主要就是提供给dispatcher模块的
            void onResponse(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)
            {
                std::string id = msg->id();
                RequestDescribe::Ptr rdp = getRequestDescribe(id);
                if (rdp == nullptr)
                {
                    ELOG("收到的响应报文id不存在");
                    return;
                }
                // 判断对应id的响应类型是异步还是回调函数
                if (rdp->_rtype == RpcType::REQ_ASYNC)
                {
                    // 如果是异步函数，那么直接将响应报文设置进去，这样另一边就可以拿到结果了
                    rdp->_response.set_value(msg);
                }
                else if (rdp->_rtype == RpcType::REQ_CALLBACK)
                {
                    // 如果是回调函数，那么就判断回调函数指针是不是空指针，如果不是，那么直接调用回调函数就行
                    if (rdp->_cb != nullptr)
                    {
                        rdp->_cb(msg);
                    }
                }
                else
                {
                    ELOG("收到未知的响应");
                }

                // 处理完成以后删除掉requestdescribe信息
                delRequestDescribe(id);
            }

            // 下面是提供发送端的接口，发送的时候使用，用来设置选择回调函数还是异步控制函数
            // 这里的send的主要逻辑就是，创建requestdescribe描述对象，然后conn发送数据就好

            // 回调函数
            bool send(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &req, const RequestCallBack &cb)
            {
                RequestDescribe::Ptr rd = createRequestDescibe(req, RpcType::REQ_CALLBACK, cb);
                if (rd.get() == nullptr)
                {
                    ELOG("创建请求描述失败");
                    return false;
                }
                conn->send(req);

                return true;
            }

            // 同步
            // 这里的同步是根据异步实现的，实际上就是直接拿到异步返回数据
            // 这里的异步在上层压根没使用，就是在同步这块使用了，上层都是自己重新实现下异步的
            bool send(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &req, BaseMessage::Ptr &rsp)
            {
                AsynResponse asyn_resp;
                bool ret = this->send(conn, req, asyn_resp);
                if (ret == false)
                {
                    return false;
                }
                rsp = asyn_resp.get(); // 这里就是返回的resp的请求报文
                return true;
            }

            // 异步
            bool send(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &req, AsynResponse &asyn_resp)
            {

                RequestDescribe::Ptr rd = createRequestDescibe(req, RpcType::REQ_ASYNC);
                if (rd.get() == nullptr)
                {
                    ELOG("创建请求描述失败");
                    return false;
                }

                conn->send(req);
                asyn_resp = rd->_response.get_future();
                return true;
            }

        private:
            // 下面属于是对于requestdescribe的增、删、查
            RequestDescribe::Ptr createRequestDescibe(const BaseMessage::Ptr &req, const RpcType &rtpye, const RequestCallBack &cb = RequestCallBack())
            {
                std::unique_lock<std::mutex> lock(_mutex);
                RequestDescribe::Ptr rd = std::make_shared<RequestDescribe>();
                rd->_request = req;
                rd->_rtype = rtpye;
                if (rtpye == RpcType::REQ_CALLBACK && cb)
                {
                    rd->_cb = cb;
                }
                _request_desc[req->id()] = rd;
                return rd;
            }
            RequestDescribe::Ptr getRequestDescribe(const std::string &id)
            {
                std::unique_lock<std::mutex> lock(_mutex);

                auto it = _request_desc.find(id);
                if (it == _request_desc.end())
                {
                    ELOG("找不到对应的请求描述");
                    return std::make_shared<RequestDescribe>();
                }
                return it->second;
            }
            void delRequestDescribe(const std::string &id)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _request_desc.find(id);
                if (it == _request_desc.end())
                {
                    ELOG("找不到对应的请求描述");
                    return;
                }
                _request_desc.erase(it);
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, RequestDescribe::Ptr> _request_desc; // 请求ID --- 对应的请求描述
        };
    }
}