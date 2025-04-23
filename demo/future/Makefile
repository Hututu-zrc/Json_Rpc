# 定义编译目标
all: test_async test_package_task test_promise

# 编译 async.cpp
test_async: async.cpp
	g++ async.cpp -o test_async -std=c++11 

# 编译 package_task.cpp
test_package_task: package_task.cpp
	g++ package_task.cpp -o test_package_task -std=c++11 -lpthread

# 编译promise.cpp
test_promise: promise.cpp
	g++ promise.cpp -o test_promise -std=c++11 -lpthread
# 清理生成的可执行文件
.PHONY: clean
clean:
	rm -f test_async  test_package_task test_promise