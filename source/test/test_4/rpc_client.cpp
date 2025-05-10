
#include <thread>
#include <chrono>
// #include "../../common/dispather.hpp"
// #include "../../common/detail.hpp"
#include "../../client/rpc_client.hpp"
using namespace zrcrpc;



void callback(const Json::Value &result)
{
    std::cout << "callback result: " << result.asInt() << std::endl;

    ILOG("callback result: %d", result.asInt());
}
int main()
{
    zrcrpc::client::RpcClient client(true, "127.0.0.1", 8888);

    auto req = MessageFactory::create<RpcRequest>();
    req->setId("12312");
    req->setMethod("Add");
    req->setMessageType(MType::REQ_RPC);
    Json::Value paramas;
    paramas["num1"] = 90;
    paramas["num2"] = 10;
    req->setParams(paramas);

    //存储结果的值
    Json::Value result;
    std::cout<<"开始发送连接"<<std::endl;
    // 测试同步
    client.call(req->method(), paramas, result);
    std::cout << "sync result: " << result.asInt() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 测试异步调用
    zrcrpc::client::RpcCaller::JsonAsynResponse resp_future;
    paramas["num1"] = 210;
    paramas["num2"] = 500;
    client.call(req->method(), paramas, resp_future);
    result= resp_future.get();
    std::cout << "async result: " <<result.asInt() << std::endl;

    // 测试回调调用
    paramas["num1"] = 600;
    paramas["num2"] = 500;
    client.call(req->method(), paramas, callback);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // client里面都是智能指针，没什么好析构的
    return 0;
}