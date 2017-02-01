#include <cpp_redis/cpp_redis>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include "gameobject.h"

#include <thread>
#define THR std::this_thread::get_id()

using namespace cpp_redis;
using namespace std;

redis_client redis;
redis_subscriber subscriptions;

void move_object(gameobject& g) {
  g.x = (g.x + g.dx) & ((1 << WORLDBITS) - 1);
  g.y = (g.y + g.dy) & ((1 << WORLDBITS) - 1);
}

void tick_object(gameobject& g) {
  ++g.age;
  
  string old_block_id = block_id(g);
  move_object(g);
  string new_block_id = block_id(g);

  if (old_block_id == new_block_id) {
    redis.set("obj:" + to_string(g.id), serialize(g));
    // TODO: mset would possibly give us more performance here
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

struct Block {
  string block_id;
  vector<int> add_object_ids;
  unordered_set<int> del_object_ids;
  unordered_map<int, gameobject> gameobjects;
  
  Block(const string& id_): block_id(id_) {
    redis.smembers("block:" + block_id, [&](auto& reply) {
        for (auto& r : reply.as_array()) {
          add_object_ids.push_back(stoi(r.as_string()));
        }
      });

    subscriptions
      .subscribe("block_add:" + block_id, [&](const string&, const string& msg) { cout << THR << " block_add;\n"; add_object_ids.push_back(stoi(msg)); })
      .subscribe("block_rem:" + block_id, [&](const string&, const string& msg) { cout << THR << " block_rem;\n"; del_object_ids.insert(stoi(msg)); })
      .commit();
  }

  void add_objects() {
    if (add_object_ids.empty()) { return; }
    
    vector<string> keys;
    std::transform(add_object_ids.begin(), add_object_ids.end(),
                   back_inserter(keys),
                   [](int id) { return "obj:"s + to_string(id); });
    redis.mget(keys, [&](auto& reply) {
        auto& array = reply.as_array();
        for (int i = 0; i != add_object_ids.size(); i++) {
          int id = add_object_ids[i];
          deserialize(gameobjects[id], array[i].as_string());
          cout << THR << "Block " << block_id << " ADD " << serialize(gameobjects[id]) << ";\n";
        }
      });
    redis.sync_commit();
    
    add_object_ids.clear();
  }

  void del_objects() {
    if (del_object_ids.empty()) { return; }

    for (auto& id : del_object_ids) {
      cout << THR << "Block " << block_id << " DEL " << serialize(gameobjects[id]) << ";\n";
      gameobjects.erase(id);
    }
    del_object_ids.clear();
  }
  
  void tick() {
    del_objects();
    add_objects();
    for (auto& id_obj : gameobjects) {
      if (id_obj.first == 0 || id_obj.second.id == 0) throw "object id 0";
      tick_object(id_obj.second);
    }
  }
  
  ~Block() {
    subscriptions
      .unsubscribe("block_add:" + block_id)
      .unsubscribe("block_rem:" + block_id)
      .commit();
  }
};


vector<Block*> blocks_write;
// TODO: neighboring blocks

int main(int argc, char* argv[]) {
  redis.connect("127.0.0.1", 6379);
  subscriptions.connect("127.0.0.1", 6379);

  string server_id = "0"s;
  if (argc > 1) {
    server_id = string(argv[1]);
  }

  cout << "starting server:" << server_id << "\n";
  
  unordered_set<string> my_blocks;
  redis.smembers("server:" + server_id, [&](auto& reply) {
      for (auto& r : reply.as_array()) {
        my_blocks.insert(r.as_string());
      }
    });
  redis.sync_commit();

  for (auto& block_id : my_blocks) {
    blocks_write.push_back(new Block(block_id));
  }
  
  for(;;) {
    cout << "simulating " << blocks_write.size() << " blocks:";
    for (auto& b : blocks_write) {
      cout << " " << b->gameobjects.size();
    }
    cout << ";\n";
    
    for (auto& b : blocks_write) {
      b->tick();
    }
    redis.sync_commit();
    usleep(1000000/10);
  }
}
