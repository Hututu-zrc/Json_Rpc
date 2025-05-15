#include "../../client/rpc_client.hpp"


int  main()
{
    auto client=std::make_shared<zrcrpc::client::TopicClient>("127.0.0.1",8888);
    //发布者应该不需要创建主题,因为默认来讲，应该是先启动订阅者，然后发布者直接对对应的主题进行发布就好了
    //这里所有的主题管理都在中转中心，和之前的服务调用不一样的

    // auto ret=client->create("hello");
    // if(ret==false)
    // {
    //     ELOG("创建主题失败");
    //     return 0;
    // }
    for(int i=0;i<5;i++)
    {
        DLOG("第%d次发布消息",i);
        client->publish("hello","world"+std::to_string(i));
    }
    //关闭连接`
    client->shutdown();
    return 0;
}