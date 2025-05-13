#include "../../client/rpc_client.hpp"


int  main()
{
    auto client=std::make_shared<zrcrpc::client::TopicClient>("127.0.0.1",8888);
    auto ret=client->create("hello");
    if(ret==false)
    {
        ELOG("创建主题失败");
        return 0;
    }
    for(int i=0;i<5;i++)
    {
        DLOG("第%d次发布消息",i);
        client->publish("hello","world"+std::to_string(i));
    }
    //关闭连接
    client->shutdown();
    return 0;
}