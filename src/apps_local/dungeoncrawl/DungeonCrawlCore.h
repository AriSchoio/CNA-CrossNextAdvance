#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>

namespace dungeoncrawl {

inline constexpr uint16_t kCatalogSize = 1024;
inline constexpr uint8_t kInventorySize = 24;
inline constexpr uint8_t kSkillCount = 12;

struct State {
  uint32_t seed = 1;
  uint32_t xp = 0;
  uint32_t coins = 25;
  uint32_t kills = 0;
  uint16_t health = 100;
  uint16_t maxHealth = 100;
  uint16_t mana = 50;
  uint16_t maxMana = 50;
  uint16_t viewers = 100;
  uint16_t rating = 50;
  uint16_t floor = 1;
  uint16_t room = 1;
  uint16_t level = 1;
  uint16_t deaths = 0;
  uint16_t inventoryCount = 0;
  uint16_t inventory[kInventorySize]{};
  uint16_t equipped[4]{};
  uint8_t skills[kSkillCount]{};
  uint8_t classId = 0;
  char quest[48] = "Defeat 3 floor monsters";
  uint8_t questProgress = 0;
  uint8_t questGoal = 3;
  uint8_t achievements = 0;
  bool inDungeon = false;
};

inline uint32_t nextRandom(uint32_t& seed) {
  seed = seed * 1664525u + 1013904223u;
  return seed;
}

inline uint32_t randomRange(uint32_t& seed, uint32_t range) {
  return range ? nextRandom(seed) % range : 0;
}

inline bool addItem(State& state, uint16_t item) {
  if (state.inventoryCount >= kInventorySize) return false;
  state.inventory[state.inventoryCount++] = item;
  return true;
}

inline const char* className(uint8_t id) {
  static const char* names[] = {"Prismatic Knight", "Dungeon Medic", "Nightblade", "System Engineer"};
  return names[id % 4];
}

inline const char* itemName(uint16_t id) {
  static const char* prefixes[] = {
      "Ash", "Prismatic", "Iron", "Moonlit", "Royal", "Cursed", "Ancient", "Storm",
      "Cracked", "Golden", "Void", "Runic", "Feral", "Solar", "Crystal", "Crawler"};
  static const char* types[] = {"Sword", "Armor", "Ring", "Potion", "Staff", "Shield", "Relic", "Charm",
                                "Boots", "Key", "Grenade", "Helmet"};
  static const char* suffixes[] = {
      "of Balance", "of Fortune", "of Hunger", "of the Floor", "of Returns", "of Sparks",
      "of Mercy", "of Shadows", "of the Sponsor", "of Defiance", "of Echoes", "of the Vault",
      "of Embers", "of Luck", "of Kings", "of Tenacity"};
  static char buffer[64];
  const uint16_t catalogId = id % kCatalogSize;
  std::snprintf(buffer, sizeof(buffer), "%s %s %s", prefixes[catalogId / 384 % 16], types[catalogId / 16 % 12],
                suffixes[catalogId % 16]);
  return buffer;
}

inline uint16_t itemPower(uint16_t id) {
  return static_cast<uint16_t>((id % kCatalogSize) / 8 + 1);
}

inline const char* skillName(uint8_t id) {
  static const char* names[] = {"Vitality", "Might", "Arcana", "Focus", "Scavenge", "Fortune",
                                "Tactics", "Alchemy", "Survival", "Leadership", "Sponsor Aura", "Loot Sense"};
  return names[id % kSkillCount];
}

inline void levelUp(State& state) {
  if (state.xp < static_cast<uint32_t>(state.level * 100)) return;
  state.xp -= state.level * 100;
  ++state.level;
  state.maxHealth += 12;
  state.maxMana += 8;
  state.health = state.maxHealth;
  state.mana = state.maxMana;
}

inline void newGame(State& state, uint32_t seed) {
  std::memset(&state, 0, sizeof(state));
  state.seed = seed ? seed : 1;
  state.health = state.maxHealth = 100;
  state.mana = state.maxMana = 50;
  state.viewers = 100;
  state.rating = 50;
  state.floor = state.room = 1;
  state.level = 1;
  state.coins = 25;
  state.inDungeon = true;
  addItem(state, static_cast<uint16_t>(randomRange(state.seed, kCatalogSize)));
  addItem(state, 64u);
}

inline void advanceRoom(State& state) {
  ++state.room;
  if (state.room > 12) {
    state.room = 1;
    ++state.floor;
    state.health = static_cast<uint16_t>(state.health + state.maxHealth / 4 < state.maxHealth
                                             ? state.health + state.maxHealth / 4
                                             : state.maxHealth);
  }
}

inline bool fight(State& state) {
  const uint32_t roll = randomRange(state.seed, 100);
  const uint16_t enemyPower = static_cast<uint16_t>(5 + state.floor * 3 + randomRange(state.seed, 12));
  const uint16_t playerPower = static_cast<uint16_t>(8 + state.level * 4 + state.skills[1] * 3);
  ++state.kills;
  ++state.questProgress;
  if (playerPower + roll / 3 >= enemyPower) {
    const uint32_t reward = 8u + state.floor * 4u + randomRange(state.seed, 10u);
    state.coins += reward;
    state.xp += 20u + state.floor * 5u;
    state.viewers = static_cast<uint16_t>(state.viewers + randomRange(state.seed, 8u));
    state.rating = static_cast<uint16_t>(state.rating + randomRange(state.seed, 5u));
    const uint16_t loot = static_cast<uint16_t>(randomRange(state.seed, kCatalogSize));
    addItem(state, loot);
    levelUp(state);
    return true;
  }
  state.health = state.health > enemyPower ? state.health - enemyPower : 0;
  state.viewers = state.viewers > 5 ? state.viewers - 5 : 0;
  if (state.health == 0) {
    ++state.deaths;
    state.health = state.maxHealth;
    state.floor = 1;
    state.room = 1;
    state.coins /= 2;
  }
  return false;
}

}  // namespace dungeoncrawl
