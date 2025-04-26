#include "net.hpp"

using namespace zrcrpc;
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
    auto resp = MessageFactory::create<RpcResponse>();
    resp->setId("12312");
    resp->setMessageType(MType::RSP_RPC);
    resp->setResponseCode(RCode::OK);
    Json::Value result;
    result["result"] = 100;
    resp->setResult(result);
    conn->send(resp);
}
int main()
{
    auto server = ServerFactory::create(8888);
    server->setMessageCallback(onMessage);
    server->start();
    return 0;
}