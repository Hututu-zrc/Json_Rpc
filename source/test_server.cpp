#include "dispather.hpp"

using namespace zrcrpc;

//这里的回调函数传入的都是基类的回调函数
void onRpcMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)//发送消息的时候调用这个函数
{
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
void onTopicMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)//发送消息的时候调用这个函数
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
    //设置回调函数
    auto dispatcher=std::make_shared<Dispatcher> ();
    dispatcher->registryCallBack(MType::REQ_TOPIC,onTopicMessage);
    dispatcher->registryCallBack(MType::REQ_RPC,onRpcMessage);
    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    auto server = ServerFactory::create(8888);
    server->setMessageCallback(message_cb);
    server->start();
    return 0;
}