#include "message.hpp"
int main()
{
    // 这样的操作一般不被允许的，这里的逻辑是将创建的子类智能指针改变为父类的BaseMessage指针 这一步是没问题
    // 将父类的BaseMessage指针又重新赋值给子类的RpcRequest指针，这一步会导致编译器识别，父类对象不能完整初始化子类对象，导致错误
    // 解决办法也有使用类型转换 dynamic_pointer_cast函数就是将父类指针转换为子类指针，内部会校验，如果是子类对象就可以转换

    // //这里是使用方法一的create
    // zrcrpc::BaseMessage::ptr bptr=zrcrpc::MessageFactory::create(zrcrpc::MType::REQ_RPC);
    // zrcrpc::RpcRequest::ptr rptr=std::dynamic_pointer_cast<zrcrpc::RpcRequest>(bptr);

    // 方法二的create函数
    // 这里就是直接将对应的子类指针传递回来，不需要进行类型转换了
    // zrcrpc::RpcRequest::ptr rptr = zrcrpc::MessageFactory::create<zrcrpc::RpcRequest>();

    // Json::Value param;
    // param["num1"] = 11;
    // param["num2"] = 22;
    // rptr->Setmethod("Add");
    // rptr->SetParam(param);
    // std::string str = rptr->Serialize();
    // std::cout << str << std::endl;

    // zrcrpc::BaseMessage::ptr bptr = zrcrpc::MessageFactory::create(zrcrpc::MType::REQ_RPC);
    // zrcrpc::RpcRequest::ptr rptr2 = std::dynamic_pointer_cast<zrcrpc::RpcRequest>(bptr);
    // bool ret=rptr2->Deserialize(str);
    // if(!ret)    return -1;
    // std::cout << rptr2->Method() << std::endl;
    // Json::Value rparam = rptr2->Param();
    // std::cout << rparam["num1"].asInt() << std::endl;
    // std::cout << rparam["num2"].asInt() << std::endl;

    // zrcrpc::TopicRequest::ptr tptr = zrcrpc::MessageFactory::create<zrcrpc::TopicRequest>();
    // tptr->SetOptype(zrcrpc::TopicOptype::TOPIC_PUBLISH);
    // tptr->Setkey("new");
    // tptr->Setmsg("hello world");
    // std::string str2=tptr->Serialize();
    // zrcrpc::BaseMessage::ptr bptr = zrcrpc::MessageFactory::create(zrcrpc::MType::PEQ_TOPIC);
    // zrcrpc::TopicRequest::ptr tptr2 = std::dynamic_pointer_cast<zrcrpc::TopicRequest>(bptr);
    // bool ret2=tptr2->Deserialize(str2);
    // if(!ret2)
    //     return -1;
    // if(tptr2->check()==false)
    // {
    //     std::cout<<"failed"<<std::endl;
    //     return -1;
    // }
    // std::cout<<tptr2->Key()<<std::endl;
    // std::cout<<(int)tptr2->Optype()<<std::endl;
    // std::cout<<tptr2->Msg()<<std::endl;

    // zrcrpc::ServiceRequest::ptr sptr = zrcrpc::MessageFactory::create<zrcrpc::ServiceRequest>();
    // sptr->SetMethod("adf");
    // // Json::Value Host;
    // // Host[KEY_HOST_IP]="127.0.0.1";
    // // Host[KEY_HOST_PORT]=8888;
    // sptr->Sethost(std::make_pair<std::string, int>("127.0.0.1", 8888));
    // sptr->SetOptype(zrcrpc::ServiceOptype::SERVICE_REGISTRY);
    // std::string str2 = sptr->Serialize();
    // zrcrpc::BaseMessage::ptr bptr = zrcrpc::MessageFactory::create(zrcrpc::MType::REQ_SERVICE);
    // zrcrpc::ServiceRequest::ptr sptr2 = std::dynamic_pointer_cast<zrcrpc::ServiceRequest>(bptr);
    // bool ret2 = sptr2->Deserialize(str2);
    // if (!ret2)
    //     return -1;
    // if (sptr2->check() == false)
    // {
    //     std::cout << "failed" << std::endl;
    //     return -1;
    // }
    // std::cout << sptr2->Method() << std::endl;
    // std::cout << (int)sptr2->Optype() << std::endl;
    // std::pair<std::string, int> host;
    // host = sptr2->Host();
    // std::cout << host.first << std::endl;
    // std::cout << host.second << std::endl;

    
    zrcrpc::ServiceResponse::Ptr service_response_ptr = zrcrpc::MessageFactory::create<zrcrpc::ServiceResponse>();

    service_response_ptr->setMethod("add");
    service_response_ptr->setOperationType(zrcrpc::ServiceOptype::SERVICE_DISCOVERY);
    service_response_ptr->setResponseCode(zrcrpc::RCode::OK);
    std::vector<zrcrpc::Address> addresses;
    addresses.emplace_back("127.0.0.1", 8888);
    addresses.emplace_back("127.0.0.1", 8889);
    service_response_ptr->setHosts(addresses);
    std::string serialized_data = service_response_ptr->serialize();
    std::cout<<serialized_data<<std::endl;

    zrcrpc::BaseMessage::Ptr base_message_ptr = zrcrpc::MessageFactory::create(zrcrpc::MType::RSP_SERVICE);
    zrcrpc::ServiceResponse::Ptr service_response_ptr2 = std::dynamic_pointer_cast<zrcrpc::ServiceResponse>(base_message_ptr);
    bool is_deserialized = service_response_ptr2->deserialize(serialized_data);
    if (!is_deserialized) {
        return -1;
    }
    if (!service_response_ptr2->isValid()) {
        std::cout << "failed" << std::endl;
        return -1;
    }
    std::cout << service_response_ptr2->method() << std::endl;
    std::cout << static_cast<int>(service_response_ptr2->operationType()) << std::endl;
    std::vector<zrcrpc::Address> retrieved_addresses = service_response_ptr2->hosts();
    for (const auto& [ip, port] : retrieved_addresses) {
        std::cout << ip << " " << port << std::endl;
    }
   
    return 0;
}
