#include "../../client/rpc_client.hpp"

void callback(const std::string & key, const std::string &msg)
{
    ILOG("收到主题%s推送的消息%s",key.c_str(),msg.c_str());
}
int main()
{
    auto client = std::make_shared<zrcrpc::client::TopicClient>("127.0.0.1", 8888);
    //订阅者需要创建主题后才能订阅该消息
    auto ret = client->create("hello");
    if (ret == false)
    {
        ELOG("创建主题失败");
        return 0;
    }
    client->subscribe("hello",callback );
    std::this_thread::sleep_for(std::chrono::seconds(5));
    client->shutdown();
    return 0;
}