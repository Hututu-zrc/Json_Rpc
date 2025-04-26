#include "net.hpp"
#include <thread>
#include <chrono>
using namespace zrcrpc;
void onMessage(const BaseConnection::Ptr &conn, const BaseMessage::Ptr &msg)
{
    std::string body = msg->serialize();
    std::cout << body << std::endl;
  
}
int main()
{
    auto client = ClientFactory::create("127.0.0.1",8888);
    client->setMessageCallback(onMessage);
    client->connect();

    auto req = MessageFactory::create<RpcRequest>();
    req->setId("12312");
    req->setMessageType(MType::REQ_RPC);
    Json::Value paramas;
    paramas["num1"] = 90;
    paramas["num2"] = 10;
    req->setParams(paramas);
    req->setMethod("Add");
    client->send(req);
    std::this_thread::sleep_for(std::chrono::seconds(10));






    
    client->shutdown();
    return 0;
}