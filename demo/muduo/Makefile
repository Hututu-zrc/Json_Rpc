INCLUDEFLAG=-std=c++11 -I ../../build/release-install-cpp11/include/
LIBFLAG= -L../../build/release-install-cpp11/lib/ -lmuduo_net -lmuduo_base -lpthread
all:server client
server:server.cpp
	g++ $(INCLUDEFLAG) $^ -o $@ $(LIBFLAG)
client:client.cpp
	g++ $(INCLUDEFLAG) $^ -o $@ $(LIBFLAG)
.PHONY:clean
clean:
	rm -f server client