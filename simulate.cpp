#include <cpp_redis/cpp_redis>
#include <set>
#include <iostream>
#include <sstream>
#include "gameobject.h"

using namespace cpp_redis;
using namespace std;

redis_client redis;

void move_object(gameobject& g) {
  g.x = (g.x + g.dx) & ((1 << WORLDBITS) - 1);
  g.y = (g.y + g.dy) & ((1 << WORLDBITS) - 1);
}

void tick_object(gameobject& g) {
  string old_block_id = block_id(g);
  move_object(g);
  string new_block_id = block_id(g);

  if (old_block_id == new_block_id) {
    redis.set("obj:" + to_string(g.id), serialize(g));
  } else {
    redis
      .multi()
      .set("obj:" + to_string(g.id), serialize(g))
      .smove("block:" + old_block_id,
             "block:" + new_block_id,
             to_string(g.id))
      .publish("block_rem:" + old_block_id, to_string(g.id))
      .publish("block_add:" + new_block_id, to_string(g.id))
      .exec();
  }
}

int main() {
  redis.connect("127.0.0.1", 6379, [](auto&) {
    cout << "client disconnected (disconnection handler)\n";
  });

  set<string> my_blocks;
  redis.smembers("server:0", [&](auto& reply) {
      for (auto& r : reply.as_array()) {
        my_blocks.insert(r.as_string());
      }
    });
  redis.sync_commit();

  vector<int> my_object_ids;
  for (auto bid : my_blocks) {
    redis.smembers("block:"+bid, [&](auto& reply) {
        for (auto& r : reply.as_array()) {
          my_object_ids.push_back(stoi(r.as_string()));
        }
      });
  }
  redis.sync_commit();

  vector<string> my_object_id_keys;
  std::transform(my_object_ids.begin(), my_object_ids.end(),
                 back_inserter(my_object_id_keys),
                 [](int id) { return "obj:"s + to_string(id); });
  redis.mget(my_object_id_keys, [&](auto& reply) {
      for (auto& r : reply.as_array()) {
        G.emplace_back();
        deserialize(G.back(), r.as_string());
        cout << "Read in " << serialize(G.back()) << ";\n";
      }
    });
  redis.sync_commit();
  
  // TODO: subscribe to block changes

  for (int tick = 0; tick < 10; tick++) {
    for (auto& g : G) {
      tick_object(g);
    }
    redis.sync_commit();
  }
  
}
