#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"

namespace zrcrpc
{
    namespace client
    {
        class Reuqestor
        {
            public:

            using reuqestorCallBack=std::function<void(BaseMessage::Ptr &)>;//这里的作用暂时存疑
            struct RequestorDescribe
            {

            };


            private:
        };
    }
}