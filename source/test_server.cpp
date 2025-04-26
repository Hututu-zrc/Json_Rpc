#include "dispather.hpp"

using namespace zrcrpc;

// 这里的回调函数传入的都是BaseMessage,基类不能直接调用子类的方法
// 客户端发送的是Request请求消息，这里接收就是Request消息，然后根据Request消息构造Response函数进行处理
void onRpcMessage(const BaseConnection::Ptr &conn, const RpcRequest::Ptr &msg)
{
    // 比如这里知道传入的RpcMessage类，但是我们拿到的父类指针，只能调用子类重写的函数，不能调用子类独有的方法
    // 这里可以采用 RpcRequest::Ptr rmsg=std::dynamic_pointer_cast<RpcRequest>(msg);

    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto resp = MessageFactory::create<RpcResponse>();
    resp->setId("12312");
    resp->setMessageType(MType::RSP_RPC);
    resp->setResponseCode(RCode::OK);
    Json::Value result;
    result["result"] = 100;
    resp->setResult(result);
    conn->send(resp);
}
void onTopicMessage(const BaseConnection::Ptr &conn, const TopicRequest::Ptr &msg) // 发送消息的时候调用这个函数
{
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto resp = MessageFactory::create<TopicResponse>();
    resp->setId("12313");
    resp->setMessageType(MType::RSP_TOPIC);
    resp->setResponseCode(RCode::OK);
    conn->send(resp);
}
int main()
{
    // 设置回调函数
    auto dispatcher = std::make_shared<Dispatcher>();
    dispatcher->registryCallBack<TopicRequest>(MType::REQ_TOPIC, onTopicMessage);
    dispatcher->registryCallBack<RpcRequest>(MType::REQ_RPC, onRpcMessage);
    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    auto server = ServerFactory::create(8888);
    server->setMessageCallback(message_cb);
    server->start();
    return 0;
}