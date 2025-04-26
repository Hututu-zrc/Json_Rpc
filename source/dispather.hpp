#pragma once
#include "net.hpp"
#include "message.hpp"
#include <unordered_map>

/*
    该模块的核心就是根据消息类型，提供对应的消息处理回调函数即可
*/
namespace zrcrpc
{
    class CallBack
    {
    public:
    private:
    };
    class Dispather
    {
    public:
        using Ptr = std::shared_ptr<Dispather>;
        void registryCallBack(const MType &mtype, const MessageCallback &msgcb)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _call_backs.find(mtype);
            if (it == _call_backs.end())
            {
                _call_backs[mtype] = msgcb;
                DLOG("回调函数注册完毕");
                return;
            }
        }

        // 该onMessage函数就是向外部提供的，根据传入的消息类型，找到自己的回调函数，传入参数，返回即可
        MessageCallback onMessage(BaseConnection::Ptr &conn, BaseMessage::Ptr &msg)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _call_backs.find(msg->messageType());
            if(it==_call_backs.end())
            {
                ELOG("回调函数未注册");
                conn->shutdown();
                return nullptr;
            }

            return it->second(conn,msg);


        }

    private:
        std::mutex _mutex; // 为了保护下面的哈希映射
        std::unordered_map<MType, MessageCallback> _call_backs;
    };
}