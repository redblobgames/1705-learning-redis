#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <vector>
#include <sstream>

const int WORLDBITS = 10; // 1<<WORLDBITS tiles on a side in the world
const int WORLDMAX = (1 << WORLDBITS) - 1;
const int BLOCKBITS = 1;  // 1<<BLOCKBITS blocks on a side in the world
const int NUMBLOCKS = (1 << BLOCKBITS) * (1 << BLOCKBITS);

struct gameobject {
  int id, x, y, dx, dy, age;

  std::string block_id() {
    if (!(0 <= x && x <= WORLDMAX)) throw "out of range";
    if (!(0 <= y && y <= WORLDMAX)) throw "out of range";
    int shift = WORLDBITS - BLOCKBITS;
    int LIMIT = 1 << BLOCKBITS;
    int bx = x >> shift, by = y >> shift;
    return std::to_string(bx) + "," + std::to_string(by);
  }
};


std::string serialize(const gameobject& g) {
  std::stringstream stream;
  stream << g.id << ' ' << g.x << ' ' << g.y << ' ' << g.dx << ' ' << g.dy << ' ' << g.age;
  return stream.str();
}

void deserialize(gameobject& g, const std::string& s) {
  std::stringstream stream(s);
  stream >> g.id >> g.x >> g.y >> g.dx >> g.dy >> g.age;
}

#endif
