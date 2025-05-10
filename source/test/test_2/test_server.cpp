#include "../../common/detail.hpp"
#include "../../server/rpc_router.hpp"
#include "../../common/dispather.hpp"
using namespace zrcrpc;

// 这里的回调函数传入的都是BaseMessage,基类不能直接调用子类的方法.
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

void Add(const Json::Value &params, Json::Value &result)
{
    int res = params["num1"].asInt() + params["num2"].asInt();
    result = res;
    std::cout << "result: " << res << std::endl;
}
int main()
{
    // 设置回调函数
    auto dispatcher = DispatcherFactory::create();

    // 这里的message_cb是提供给server的回调函数
    auto message_cb = std::bind(&zrcrpc::Dispatcher::onMessage, dispatcher.get(),
                                std::placeholders::_1, std::placeholders::_2);

    // 设置对应的服务
    auto sd = std::make_shared<zrcrpc::server::SDFactory>();
    sd->setServiceName("Add");
    sd->setParamsDesc("num1", zrcrpc::server::ParamType::INTEGRAL);
    sd->setParamsDesc("num2", zrcrpc::server::ParamType::INTEGRAL);
    sd->setRtype(zrcrpc::server::ParamType::INTEGRAL);
    sd->setServiceServiceCallBack(Add);

    auto router = std::make_shared<zrcrpc::server::Rpc_Router>();

    // 向router里面注册对应的服务
    router->registryMethod(sd->build());

    // 这里的router_cb是提供给dispatcher的回调函数
    auto router_cb = std::bind(&zrcrpc::server::Rpc_Router::onRequest, router.get(),
                               std::placeholders::_1, std::placeholders::_2);

    // 这里需要理解为什么注册到dispatcher模块里面是rpcrequest消息，因为这个回调函数处理的就是rpcrequest消息
    dispatcher->registryCallBack<RpcRequest>(zrcrpc::MType::REQ_RPC, router_cb);

    auto server = ServerFactory::create(8888);
    server->setMessageCallback(message_cb);
    server->start();
    return 0;
}