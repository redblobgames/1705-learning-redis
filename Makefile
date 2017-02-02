CPPFLAGS=-g -O2 -std=c++14 -I deps/include
LDFLAGS=-L deps/lib -lcpp_redis -ltacopie

all: bin/create bin/simulate

bin/%: %.cpp gameobject.h Makefile
	c++ $(CPPFLAGS) $< $(LDFLAGS) -o $@
