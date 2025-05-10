#pragma once
#include "net.hpp"
#include "message.hpp"
#include <unordered_map>

/*
    该模块的核心就是根据消息类型，提供对应的消息处理回调函数即可
    判断registryCallBack<T>实例化是什么类型，就看注册的函数时干嘛的，如果是处理服务响应的，那就T=SERVICE_RESPONSE
*/
namespace zrcrpc
{

    class CallBack
    {
    public:
        using Ptr = std::shared_ptr<CallBack>;
        virtual void onMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg) = 0;

    private:
    };

    template <typename T> // 这里的T是消息的具体的类比如RpcRequest
    class CallBackTemplate : public CallBack
    {
    public:
        using Ptr = std::shared_ptr<CallBackTemplate<T>>;
        // 解决传入的回调函数类型不一致的核心就是这里，这里的是shared_ptr<T>根据传入的消息类型，固定消息的指针
        using MessageCallback = std::function<void(const BaseConnection::Ptr &, const std::shared_ptr<T> &)>;
        CallBackTemplate(const MessageCallback &handler)
            : _handler(handler)
        {
        }
        virtual void onMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg) override
        {
            auto type_msg = std::dynamic_pointer_cast<T>(msg);
            _handler(conn, type_msg);
        }

    private:
        MessageCallback _handler;
    };

        class Dispatcher
    {
    public:
        using Ptr = std::shared_ptr<Dispatcher>;

        // 这里的T代表对应消息的具体的类，比如RpcRequest、TopicResponse等等
        // 这里如果是模板，那么就会插入多种类型到哈希map里面，这样不允许的
        // 为了解决这样的问题，将T进行封装，封装到一个CallBack类里面，这样就不会存在多种的T了
        // 但是这样发现，传入的callback还是会因为T的不同生成不同类型的T
        // 解决办法就是再次使用多态，父类指针指向由模板实例化出来的不同类型的T，然后调用函数，就不在单独传入类了，传入类的指针去调用
        template <typename T> // 下面应该根据传入的函数类型构造响应的回调函数
        void registryCallBack(const MType &mtype, const typename CallBackTemplate<T>::MessageCallback &handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _call_backs.find(mtype);
            if (it == _call_backs.end())
            {
                auto cb = std::make_shared<CallBackTemplate<T>>(handler);
                _call_backs[mtype] = cb;
                DLOG("回调函数注册完毕");
                return;
            }
        }

        // 该onMessage函数就是向外部提供的，根据传入的消息类型，找到自己的回调函数，传入参数，返回即可
        void onMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _call_backs.find(msg->messageType());
            if (it == _call_backs.end())
            {
                ELOG("回调函数未注册");
                conn->shutdown();
                return;
            }
            // 这里调用了callbacktemplate里面的onmessage去设置成员变量
            it->second->onMessage(conn, msg);
            return;
        }

    private:
        std::mutex _mutex; // 为了保护下面的哈希映射
        std::unordered_map<MType, CallBack::Ptr> _call_backs;
    };

    class DispatcherFactory
    {
    public:
        template <typename... Args>
        static Dispatcher::Ptr create(Args... args)
        {
            return std::make_shared<Dispatcher>(std::forward<Args>(args)...);
        }

    private:
    };
}