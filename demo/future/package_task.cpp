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

    // 一般的使用步骤就是
    // (1)、封装任务
    // (2)、获得future
    // (3)、执行任务
    // (4)、获取结果

    // 1、在本线程中执行

    // //(1)
    // std::packaged_task<int(int, int)> task(add);
    // //(2)
    // std::future<int> res = task.get_future();
    // //(3)
    // task(1024, 1024); // 本线程里面执行，如果不执行，下面直接获取get，会阻塞住
    // //(4)
    // std::cout << "way1: " << res.get() << "\n";

    // 2、在另一个线程里面执行
    // 2.1 在使用thread库里面直接传入
    //  由于 std::packaged_task 管理着内部的状态和资源（如任务的执行上下文、结果存储等），
    //  为了避免多个 std::packaged_task 对象同时管理同一份资源而导致资源竞争和不一致的问题
    // std::packaged_task<int(int, int)> task(add); // 不能被拷贝，只能移动
    // std::future<int> res = task.get_future();
    // std::thread td(std::move(task), 1024, 1024);
    // std::cout << "way2.1: " << res.get() << "\n";
    // td.join();

    // 2.2 使用智能指针去指向函数，这等会就传递指针
    auto task= std::make_shared<std::packaged_task<int(int, int)>> (add);// 不能被拷贝，只能移动
    std::future<int> res = task->get_future();
    std::thread td([task](){
        task->operator()(100,250);
    });
    std::cout << "way2.1: " << res.get() << "\n";
    td.join();
}