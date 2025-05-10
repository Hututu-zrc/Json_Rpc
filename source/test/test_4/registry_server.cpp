
#include "../../server/rpc_server.hpp"

using namespace zrcrpc;


int main()
{


    //打开注册中心服务器
    zrcrpc::server::RegistryServer reg_server(8888);
    reg_server.start();

    return 0;
}