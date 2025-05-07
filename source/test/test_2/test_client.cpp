
#include <thread>
#include <chrono>
#include "../../common/dispather.hpp"
#include "../../common/detail.hpp"
#include "../../client/rpc_caller.hpp"
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
    auto dispatcher = DispatcherFactory::create();

    auto requestor = std::make_shared<zrcrpc::client::Reuqestor>();
    auto rpc_caller = std::make_shared<zrcrpc::client::RpcCaller>(requestor);

    // 这里收到的是server发来的response响应报文
    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, dispatcher.get(),
                                std::placeholders::_1, std::placeholders::_2);

    // 提供给dispatcher模块的回调函数
    auto requestor_cb = std::bind(&zrcrpc::client::Reuqestor::onResponse, requestor.get(),
                                  std::placeholders::_1, std::placeholders::_2);
    dispatcher->registryCallBack<RpcResponse>(zrcrpc::MType::RSP_RPC, requestor_cb);

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

    Json::Value result;
    rpc_caller->call(client->connection(), req->method(), paramas, result);
    std::cout << "result: " << result.asInt() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(10));

    zrcrpc::client::RpcCaller::JsonAsynResponse resp_future;
    paramas["num1"] = 210;
    paramas["num2"] = 500;
    rpc_caller->call(client->connection(), req->method(), paramas, resp_future);
    std::cout << "result: " << resp_future.get().asInt() << std::endl;

    client->shutdown();
    return 0;
}