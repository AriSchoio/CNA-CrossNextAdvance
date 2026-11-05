#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace dungeoncrawl {

inline constexpr uint16_t kCatalogSize = 1024;
inline constexpr uint8_t kInventorySize = 24;
inline constexpr uint8_t kSkillCount = 12;
inline constexpr uint32_t kStateVersion = 0xDCC4331u;

struct State {
  uint32_t version = kStateVersion;
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
  uint16_t enemyHealth = 0;
  uint16_t enemyMaxHealth = 0;
  uint16_t enemyPower = 0;
  uint16_t guard = 0;
  uint8_t battleTurn = 0;
  uint8_t enemyIntent = 0;
  uint8_t combo = 0;
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
  state.version = kStateVersion;
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

inline const char* roomName(const State& state) {
  static const char* rooms[] = {"Antechamber", "Loot Vault", "Monster Den", "Sponsor Arena", "Trap Gallery",
                                "Medic Station", "Boss Gate", "Merchant Alcove"};
  return rooms[(state.floor + state.room) % 8];
}

inline const char* enemyName(const State& state) {
  static const char* enemies[] = {"Bonebutcher Beetle", "Dungeon Oracle", "Ash Hound", "Prism Goblin",
                                  "Vault Warden", "Crawler Mimic", "Neon Basilisk", "Sponsor Agent"};
  return enemies[(state.floor * 3u + state.room) % 8u];
}

inline void beginEncounter(State& state) {
  if (state.enemyMaxHealth != 0) return;
  state.enemyMaxHealth = static_cast<uint16_t>(18 + state.floor * 7 + randomRange(state.seed, 18u));
  state.enemyHealth = state.enemyMaxHealth;
  state.enemyPower = static_cast<uint16_t>(4 + state.floor * 2 + randomRange(state.seed, 8u));
  state.guard = 0;
  state.battleTurn = 1;
  state.combo = 0;
  state.enemyIntent = static_cast<uint8_t>(randomRange(state.seed, 3u));
}

inline bool usePotion(State& state) {
  for (uint16_t i = 0; i < state.inventoryCount; ++i) {
    if ((state.inventory[i] % 12u) == 3u || (state.inventory[i] % 12u) == 6u) {
      state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 28u));
      for (uint16_t n = i + 1; n < state.inventoryCount; ++n) state.inventory[n - 1] = state.inventory[n];
      --state.inventoryCount;
      return true;
    }
  }
  state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 12u));
  return true;
}

enum class BattleAction : uint8_t { Attack = 0, Guard = 1, Arcane = 2, Item = 3, Escape = 4 };

inline bool battleTurn(State& state, BattleAction action) {
  if (state.enemyMaxHealth == 0) beginEncounter(state);
  if (action == BattleAction::Escape) {
    if (randomRange(state.seed, 100u) < 35u + state.skills[6] * 2u) {
      state.enemyHealth = 0;
      state.enemyMaxHealth = 0;
      state.guard = 0;
      state.combo = 0;
      return true;
    }
  }

  uint16_t damage = 0;
  if (action == BattleAction::Attack) {
    damage = static_cast<uint16_t>(10 + state.level * 2 + state.skills[1] * 3 + randomRange(state.seed, 8u));
    damage += state.combo * 2u;
    state.combo = static_cast<uint8_t>(std::min<uint32_t>(5u, state.combo + 1u));
  } else if (action == BattleAction::Arcane) {
    if (state.mana >= 14u) {
      state.mana -= 14u;
      damage = static_cast<uint16_t>(16 + state.level * 3 + state.skills[2] * 4 + randomRange(state.seed, 10u));
      state.combo = 0;
    } else {
      damage = 3u;
      state.combo = 0;
    }
  } else if (action == BattleAction::Item) {
    usePotion(state);
    state.combo = 0;
  } else if (action == BattleAction::Guard) {
    state.guard = static_cast<uint16_t>(12 + state.level * 2 + state.skills[0] * 2);
    state.mana = static_cast<uint16_t>(std::min<uint32_t>(state.maxMana, state.mana + 8u));
    state.combo = 0;
  }

  if (damage >= state.enemyHealth) state.enemyHealth = 0;
  else state.enemyHealth -= damage;
  if (state.enemyHealth == 0) {
    const uint32_t reward = 10u + state.floor * 5u + randomRange(state.seed, 12u);
    state.coins += reward;
    state.xp += 24u + state.floor * 7u;
    state.kills++;
    if (state.questProgress < state.questGoal) ++state.questProgress;
    state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 4u + randomRange(state.seed, 8u)));
    state.rating = static_cast<uint16_t>(std::min<uint32_t>(100u, state.rating + randomRange(state.seed, 5u)));
    if (state.inventoryCount < kInventorySize)
      state.inventory[state.inventoryCount++] = static_cast<uint16_t>(randomRange(state.seed, kCatalogSize));
    levelUp(state);
    state.enemyMaxHealth = 0;
    state.enemyPower = 0;
    state.guard = 0;
    state.combo = 0;
    advanceRoom(state);
    return true;
  }

  uint16_t incoming = state.enemyPower + randomRange(state.seed, 7u);
  if (state.enemyIntent == 0) incoming += 8u;
  if (state.enemyIntent == 2) incoming = incoming / 2u;
  if (state.guard >= incoming) {
    state.guard -= incoming;
    incoming = 0;
  } else {
    incoming -= state.guard;
    state.guard = 0;
  }
  if (incoming >= state.health) state.health = 0;
  else state.health -= incoming;
  state.guard = 0;
  ++state.battleTurn;
  state.enemyIntent = static_cast<uint8_t>(randomRange(state.seed, 3u));

  if (state.health == 0) {
    ++state.deaths;
    state.health = state.maxHealth;
    state.mana = state.maxMana;
    state.floor = 1;
    state.room = 1;
    state.coins /= 2u;
    state.enemyHealth = 0;
    state.enemyMaxHealth = 0;
  }
  return false;
}

inline bool fight(State& state) {
  const uint32_t roll = randomRange(state.seed, 100);
  const uint16_t enemyPower = static_cast<uint16_t>(5 + state.floor * 3 + randomRange(state.seed, 12));
  const uint16_t playerPower = static_cast<uint16_t>(8 + state.level * 4 + state.skills[1] * 3);
  ++state.kills;
  if (playerPower + roll / 3 >= enemyPower) {
    if (state.questProgress < state.questGoal) ++state.questProgress;
    const uint32_t reward = 8u + state.floor * 4u + randomRange(state.seed, 10u);
    state.coins += reward;
    state.xp += 20u + state.floor * 5u;
    state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + randomRange(state.seed, 8u)));
    state.rating = static_cast<uint16_t>(std::min<uint32_t>(100u, state.rating + randomRange(state.seed, 5u)));
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
