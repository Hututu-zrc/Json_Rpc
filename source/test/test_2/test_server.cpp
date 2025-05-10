#include "../../common/detail.hpp"
#include "../../server/rpc_router.hpp"
#include "../../common/dispather.hpp"
using namespace zrcrpc;



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