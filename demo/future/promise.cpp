#include <iostream>
#include <unistd.h>
#include <future>
#include <thread>
#include <memory>

int main()
{
    auto add = [](int a, int b)
    {
        // sleep(1);
        std::cout << "add running" << std::endl;
        return a + b;
    };

    std::promise<int> pro;
    std::future<int> res=pro.get_future();

    std::thread td([&](){
        pro.set_value(add(10,200));
    });

    std::cout<<res.get()<<std::endl;
    td.join();
    return 0;
}