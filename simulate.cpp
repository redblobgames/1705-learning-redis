/* Simulation server
 * 
 * Copyright 2017 Red Blob Games <redblobgames@gmail.com>
 * License: Apache v2.0 <http://www.apache.org/licenses/LICENSE-2.0.html>
 *
 * Game simulation for a single server shard, using redis to store
 * game state. Runs in a "functional" style, reading from one set of
 * redis keys and writing to a different set of redis keys.
 *
 */

#include <cpp_redis/cpp_redis>
#include <utility>
#include <unistd.h>
#include "gameobject.h"

#include <iostream>
using std::cout;

#include <unordered_set>
using std::unordered_set;

#include <vector>
using std::vector;

#include <string>
using std::string; using std::to_string;


int tick = 0;
const int TICKS_PER_SECOND = 60;
cpp_redis::redis_client redis;

void tick_object(gameobject& g) {
  ++g.age;
  g.x = (g.x + g.dx) & ((1 << WORLDBITS) - 1);
  g.y = (g.y + g.dy) & ((1 << WORLDBITS) - 1);
}

struct Block {
  string block_id;
  
  Block(const string& id_): block_id(id_) {
  }

  void run() {
    vector<int> object_ids;
    vector<gameobject> gameobjects;
    
    redis.smembers("block:" + block_id + ":" + to_string(tick), [&](auto& reply) {
        for (auto& r : reply.as_array()) {
          object_ids.push_back(stoi(r.as_string()));
        }
      });
    redis.sync_commit();

    if (!object_ids.empty()) {
    
      vector<string> keys;
      transform(object_ids.begin(), object_ids.end(),
                back_inserter(keys),
                [](int id) { return "obj:" + to_string(id) + ":" + to_string(tick); });
      redis.mget(keys, [&](auto& reply) {
          for (auto& r : reply.as_array()) {
            gameobjects.emplace_back();
            deserialize(gameobjects.back(), r.as_string());
            // cout << "Block " << block_id << " has " << serialize(gameobjects.back()) << ";\n";
          }
        });
      redis.sync_commit();

      for (auto& g : gameobjects) {
        tick_object(g);
      }

      vector<std::pair<string, string>> outputs;
      transform(gameobjects.begin(), gameobjects.end(),
                back_inserter(outputs),
                [](const gameobject& g) { return std::make_pair("obj:" + to_string(g.id) + ":" + to_string(tick+1), serialize(g)); });
      redis.mset(outputs);

      // TODO: this could be a batch operation
      for (auto& g : gameobjects) {
        redis.sadd("block:" + g.block_id() + ":" + to_string(tick+1), {to_string(g.id)});
      }

    }
    
    redis.sadd("finished:" + to_string(tick), {block_id});
  }
  
  ~Block() {
  }
};


// TODO: neighboring blocks

int main(int argc, char* argv[]) {
  redis.connect("127.0.0.1", 6379);

  string server_id = "A";
  if (argc > 1) {
    server_id = string(argv[1]);
  }

  cout << "starting server: " << server_id << "\n";
  redis.del({"servertimestamp:" + server_id});
  
  for(;;) {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  
    unordered_set<string> my_blocks;
    redis.smembers("server:" + server_id + ":0", [&](auto& reply) {
        for (auto& r : reply.as_array()) {
          my_blocks.insert(r.as_string());
        }
      });
    redis.sync_commit();
  
    cout << "simulating tick " << tick << ":\n";
  
    for (auto& block_id : my_blocks) {
      Block block(block_id);
      block.run();
      // NOTE: there are still callbacks referencing the block, so I
      // can't proceed until I've processed them
      redis.sync_commit();
    }

    int frame_time_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    redis.lpush("servertimestamp:" + server_id, {to_string(frame_time_microseconds)});
    
    // Wait for all simulations to finish
    for(int blocks_finished = 1; 0 < blocks_finished && blocks_finished < NUMBLOCKS;) {
      redis.scard("finished:" + to_string(tick), [&](auto& reply) {
          blocks_finished = reply.as_integer();
        });
      redis.sync_commit();
      usleep(1000000/100);
    }

    // Wait for next tick
    frame_time_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    usleep(std::max(1, 1000000/TICKS_PER_SECOND - frame_time_microseconds));

    cout << "(" << frame_time_microseconds << ") ";
    ++tick;
    
    // TODO: do I need PEXPIRES stuff? maybe everyone will be at this
    // point together, maybe it doesn't matter as long as the previous
    // simulation finished
  }
  
}
