#include <cpp_redis/cpp_redis>
#include <iostream>
#include "gameobject.h"

const int NUM_OBJECTS = 1000;
int next_obj_id = 0; // In this test, only create.cpp creates objects

using namespace cpp_redis;
using namespace std;

redis_client redis;

int randint(int lo, int hi) {
  return random() % (hi-lo+1) + lo;
}

void random_init(gameobject& g) {
  g.x = randint(0, WORLDMAX);
  g.y = randint(0, WORLDMAX);
  g.dx = randint(-10, 10);
  g.dy = randint(-10, 10);
}

void create_object() {
  int id = ++next_obj_id;

  gameobject obj;
  obj.id = id;
  obj.age = 0;
  random_init(obj);

  redis
    .set("obj:" + to_string(id) + ":0", serialize(obj))
    .sadd("block:" + obj.block_id() + ":0", vector<string>{to_string(id)});
}

void reset_db() {
  redis.flushall();
  redis.sync_commit();
  
  // Two-server partitioning for now
  redis.sadd("server:A:0", vector<string>{"0,0"s, "0,1"s});
  redis.sadd("server:B:0", vector<string>{"1,0"s, "1,1"s});
}

int main() {
  redis.connect("127.0.0.1", 6379, [](auto&) {
    cout << "client disconnected (disconnection handler)\n";
  });

  reset_db();

  for (int i = 0; i < NUM_OBJECTS; i++) {
    create_object();
  }

  redis.sync_commit();
}
