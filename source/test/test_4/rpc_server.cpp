
#include "../../server/rpc_router.hpp"
#include "../../server/rpc_server.hpp"

using namespace zrcrpc;



void Add(const Json::Value &params, Json::Value &result)
{
    int res = params["num1"].asInt() + params["num2"].asInt();
    result = res;
    std::cout << "result: " << res << std::endl;
}
int main()
{

    // 创建服务描述
    zrcrpc::server::SDFactory::Ptr sd = std::make_shared<zrcrpc::server::SDFactory>();
    sd->setServiceName("Add");
    sd->setParamsDesc("num1", zrcrpc::server::ParamType::INTEGRAL);
    sd->setParamsDesc("num2", zrcrpc::server::ParamType::INTEGRAL);
    sd->setRtype(zrcrpc::server::ParamType::INTEGRAL);
    sd->setServiceServiceCallBack(Add);

    // 创建服务器，开启服务发先功能
    zrcrpc::server::RpcServer server(Address("127.0.0.1", 9090),true ,Address("127.0.0.1", 8888));
    server.registryMethod(sd->build());
    server.start();

    return 0;
}