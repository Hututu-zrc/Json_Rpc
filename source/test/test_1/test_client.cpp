
#include <thread>
#include <chrono>
#include "dispather.hpp"
using namespace zrcrpc;

// 这里的on***Message都是客户端向服务器发送消息以后，接收到服务器返回消息的时候调用的函数
// 发送的request消息，接收的是response消息,然后对response消息进行处理，这里就是直接打印
void onRpcMessage(const BaseConnection::Ptr &conn, const RpcResponse::Ptr &msg)
{
    DLOG("this is RpcMessage request");
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

void onTopicMessage(const BaseConnection::Ptr &conn, const TopicResponse::Ptr &msg)
{
    DLOG("this is TopicMessage request");

    std::string body = msg->serialize();
    std::cout << body << std::endl;
}
int main()
{

    // 设置回调函数
    auto dispatcher = std::make_shared<Dispatcher>();
    // 这里收到的是server发来的response响应报文
    dispatcher->registryCallBack<TopicResponse>(MType::RSP_TOPIC, onTopicMessage);
    dispatcher->registryCallBack<RpcResponse>(MType::RSP_RPC, onRpcMessage);
    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    // 设置Rpc请求
    auto client = ClientFactory::create("127.0.0.1", 8888);
    client->setMessageCallback(message_cb);
    client->connect();

    auto req = MessageFactory::create<RpcRequest>();
    req->setId("12312");
    req->setMethod("Add");
    req->setMessageType(MType::REQ_RPC);
    Json::Value paramas;
    paramas["num1"] = 90;
    paramas["num2"] = 10;
    req->setParams(paramas);

    client->send(req);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    // 设置Topic请求
    auto req2 = MessageFactory::create<TopicRequest>();
    req2->setId("12313");
    req->setMethod("Add");
    req2->setMessageType(MType::REQ_TOPIC);
    req2->setKey("topic");
    req2->setOperationType(TopicOptype::TOPIC_CREATE);
    // req2->setMessage("add");

    client->send(req2);
    std::this_thread::sleep_for(std::chrono::seconds(10));

    client->shutdown();
    return 0;
}