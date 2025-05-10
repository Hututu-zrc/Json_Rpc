CFLAG= -std=c++11 -I ../../../build/release-install-cpp11/include/
LFLAG= -L../../../build/release-install-cpp11/lib  -ljsoncpp -lmuduo_net -lmuduo_base -pthread
all : rpc_client reg_server rpc_server
reg_server :registry_server.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
rpc_server :rpc_server.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
rpc_client : rpc_client.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)

.PHONY:clean
clean:
	rm -f rpc_client reg_server rpc_server