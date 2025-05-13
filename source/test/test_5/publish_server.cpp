#include "../../server/rpc_server.hpp"


int main()
{
    auto server=std::make_shared<zrcrpc::server::TopicServer> (8888);
    server->start();
    return 0;
}