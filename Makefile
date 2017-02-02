CPPFLAGS=-g -O2 -std=c++14 -I deps/include
LDFLAGS=-L deps/lib -lcpp_redis -ltacopie
CPP_REDIS_LIB=deps/lib/libcpp_redis.a deps/lib/libtacopie.a
all: bin/create bin/simulate

bin/%: %.cpp gameobject.h $(CPP_REDIS_LIB) Makefile
	c++ $(CPPFLAGS) $< $(LDFLAGS) -o $@

$(CPP_REDIS_LIB):
	git submodule update --init --recursive
	mkdir -p cpp_redis/build
	cd cpp_redis/build \
		&& cmake -DCMAKE_INSTALL_PREFIX=../../deps -DREAD_SIZE=65536 -DIO_SERVICE_NB_WORKERS=1 .. \
		&& make install \
		&& make clean
