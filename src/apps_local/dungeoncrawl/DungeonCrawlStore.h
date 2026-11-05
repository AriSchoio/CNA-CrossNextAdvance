#pragma once

#include <HalStorage.h>

#include <cstring>

#include "DungeonCrawlCore.h"

namespace dungeoncrawl {

class Store {
 public:
  bool load(State& state) {
    HalFile file;
    if (!Storage.openFileForRead("DCCGAME", "/.crosspoint/dungeon-crawl.cfg", file)) return false;
    if (file.fileSize() != sizeof(State)) return false;
    const int result = file.read(reinterpret_cast<uint8_t*>(&state), sizeof(State));
    file.close();
    return result == static_cast<int>(sizeof(State));
  }

  bool save(const State& state) {
    HalFile file;
    if (!Storage.openFileForWrite("DCCGAME", "/.crosspoint/dungeon-crawl.cfg", file)) return false;
    const bool ok = file.write(reinterpret_cast<const uint8_t*>(&state), sizeof(State)) == sizeof(State);
    file.close();
    return ok;
  }
};

}  // namespace dungeoncrawl
