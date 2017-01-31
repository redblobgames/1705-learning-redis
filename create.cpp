#include <cpp_redis/cpp_redis>
#include <iostream>
#include "gameobject.h"

const int NUM_OBJECTS = 3;

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
  int id = -1;
  redis.incr("next_obj_id", [&](auto& reply) {
      id = reply.as_integer();
    });
  // Can't do anything else until we've gotten the id
  redis.sync_commit();

  G.emplace_back(gameobject{id, 0, 0, 0, 0});
  random_init(G.back());
  int bid = block_id(G.back());

  redis
    .multi()
    .set("obj:" + to_string(id), serialize(G.back()))
    .sadd("block:" + to_string(bid), vector<string>{to_string(id)})
    .publish("block_add:" + to_string(bid), to_string(id))
    .exec();
}

void reset_db() {
  vector<string> keys_to_reset;
  redis.set("next_obj_id", "0");
  redis.del(vector<string>{"server:0"s});
  redis.sadd("server:0", vector<string>{"0"s, "1"s, "2"s, "3"s});
  redis.keys("obj:*", [&](auto& reply) {
      for (auto& r : reply.as_array()) {
        keys_to_reset.push_back(r.as_string());
      }
    });
  redis.keys("block:*", [&](auto& reply) {
      for (auto& r : reply.as_array()) {
        keys_to_reset.push_back(r.as_string());
      }
    });
  // NOTE: we need to wait for all the queries to come back before
  // keys_to_reset has been built
  redis.sync_commit();
  redis.del(keys_to_reset);
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
