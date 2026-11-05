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
    if (result != static_cast<int>(sizeof(State)) || state.version != kStateVersion) return false;
    if (state.inventoryCount > kInventorySize || state.floor == 0 || state.level == 0 ||
        state.maxHealth == 0 || state.maxMana == 0 || state.health > state.maxHealth ||
        state.mana > state.maxMana || state.classId >= 4 || state.npcId >= 24 ||
        state.reputation > 100 || state.eventId >= 12 || state.routeId >= 4 ||
        state.questGoal == 0 || state.enemyHealth > state.enemyMaxHealth ||
        state.attributePoints > 99 || state.floor > 1000 || state.room > 12) {
      return false;
    }
    for (uint16_t i = 0; i < state.inventoryCount; ++i) {
      if (state.inventory[i] >= kCatalogSize) return false;
    }
    for (uint8_t i = 0; i < kSkillCount; ++i) {
      if (state.skills[i] > 20u) return false;
    }
    for (uint8_t i = 0; i < 4u; ++i) {
      if (state.equipped[i] >= kCatalogSize) return false;
    }
    return true;
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
