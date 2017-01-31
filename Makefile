CPPFLAGS=-g -O2 -std=c++14 -I ../../src/cpp_redis/includes -I ../../src/cpp_redis/tacopie/includes
LDFLAGS=-L ../../src/cpp_redis/build/lib -lcpp_redis -ltacopie

all: bin/create bin/simulate

bin/%: %.cpp gameobject.h Makefile
	c++ $(CPPFLAGS) $< $(LDFLAGS) -o $@
