#include <iostream>
#include <unistd.h>
#include <future>
#include <thread>
#include <chrono>

int main()
{
    auto add = [](int a, int b)
    {
        // sleep(1);
        std::cout << "add running" << std::endl;
        return a + b;
    };

    // std::async可以自己创建一个新的执行流然后执行函数，实现异步调用，返回对象就是一个future对象
    std::future<int> res1 = std::async(std::launch::async, add, 100, 200);
    // 同步的策略，只有get或者wait函数调用的时候，再去执行策略
    std::future<int> res2 = std::async(std::launch::deferred, add, 100, 200);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "-------------------" << std::endl;
    // std::future<int> ::get()就是获取异步执行的结果，如果没有结果，就阻塞等待
    std::cout << "res1:" << res1.get() << std::endl;
    std::cout << "res2:" << res2.get() << std::endl;
    return 0;
}