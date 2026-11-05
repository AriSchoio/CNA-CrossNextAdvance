#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace dungeoncrawl {

inline constexpr uint16_t kCatalogSize = 1024;
inline constexpr uint16_t kUnequippedItem = 0xffffu;
inline constexpr uint8_t kInventorySize = 24;
inline constexpr uint8_t kSkillCount = 12;
inline constexpr uint32_t kStateVersion = 0xDCC4335u;

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
  uint8_t eventId = 0;
  uint8_t routeId = 0;
  uint8_t npcId = 0;
  uint8_t reputation = 50;
  uint8_t attributePoints = 3;
  uint16_t exploredNodes = 0;
  uint8_t battleSkillCooldown = 0;
  uint8_t enemyStunned = 0;
  uint16_t playerShield = 0;
  uint16_t enemyGuard = 0;
  char crawlerName[24] = "Carl";
  uint8_t originId = 0;
  uint8_t factionId = 0;
  uint8_t appearanceId = 0;
  uint8_t chapter = 1;
  uint16_t notoriety = 0;
  uint32_t soulCoins = 0;
  uint8_t activeSkillId = 0;
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

inline const char* originName(uint8_t id) {
  static const char* names[] = {"Earth Tourist", "Bunker Survivor", "Dungeon Native", "System Rebel"};
  return names[id % 4u];
}

inline const char* factionName(uint8_t id) {
  static const char* names[] = {"Newbie Alliance", "Desperado Club", "Royal Court", "Bedlam Crew"};
  return names[id % 4u];
}

inline const char* appearanceName(uint8_t id) {
  static const char* names[] = {"Classic", "Ascendant", "Royal", "Bedlam", "Neon", "Feral"};
  return names[id % 6u];
}

inline void nameNextCrawl(State& state) {
  static const char* names[] = {"Carl", "Donut", "Mongo", "Katja", "Samantha", "Orren"};
  for (uint8_t i = 0; i < 6u; ++i) {
    if (std::strcmp(state.crawlerName, names[i]) == 0) {
      std::snprintf(state.crawlerName, sizeof(state.crawlerName), "%s", names[(i + 1u) % 6u]);
      return;
    }
  }
  std::snprintf(state.crawlerName, sizeof(state.crawlerName), "%s", names[0]);
}

inline const char* itemName(uint16_t id) {
  static const char* specialNames[] = {
      "Enchanted Broom", "Enchanted Toilet Brush", "Mongo's Bone Collar", "Mordecai's Training Signet",
      "Prepotente's Horn", "Sascha's Signet", "Samantha's Scavenger Kit", "Imani's Training Blade",
      "Juice Box Shield", "Odette's Moonblade", "Florin's Lucky Coin", "Rosetta's Resonance Key",
      "Desperado Club Card", "Faction War Banner", "Sister Ines' Prayer Lantern", "Emberus Cinder",
      "Larracos Ember Ring", "Tipid's Trick Lantern", "Rend's War Plate", "Firas' Bastion Key",
      "Bautista's Rally Drum", "Lucia Mar's Navigator Lens", "Odette's Moonstone", "Mogro's War Harness",
      "Mantes' System Token", "Santiago's Iron Will", "Desperado Healing Draught", "Courier's Guild Patch",
      "Dungeon Guild Badge", "Floor Boss Contraband", "Borant Sponsor Seal", "Quasar Loot Core",
      "Katia's Knife", "Chris's Exfiltration Map", "Rex's Survival Collar", "Princess Donut's Tiara",
      "Mongo's Chain", "Newbie Village Compass", "Achievement: Bloodbath", "Achievement: Skullcrush",
      "Lootbox Golden Key", "Necromancer's Tome", "Polymorph Potion", "Bunker Helmet",
      "Overpowered Potion", "Bounty Hunter Contract", "Earth Mother Idol", "Moon Market Token",
      "Royal Court Cipher", "Bedlam Bride's Vow", "Feral Gods Tablet", "Masquerade Mask",
      "Parade Horror Bell", "Inevitable Ruin Shard", "Butcher's Cleaver", "Anarchist Cookbook",
      "Doomsday Scenario Map", "Bedlam Bone Relic", "Quarry Key", "Train Station Token",
      "Ocean's Razor", "Grendel's Iron Tooth", "Ginger's Lucky Charm", "Socrates' Memory Core",
      "Hekla's Firebrand", "Tran's Field Kit", "Dong's Lucky Chopsticks", "Ferdinand's Royal Banner"
  };
  if (id < 64u) return specialNames[id];
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

inline bool itemIsConsumable(uint16_t id) {
  if (id < 64u) {
    return id == 26u || id == 40u || id == 41u || id == 43u || id == 44u ||
           id == 46u || id == 47u || id == 48u || id == 54u || id == 55u ||
           id == 58u || id == 62u || id == 63u;
  }
  return (id % 12u) == 3u || (id % 12u) == 6u;
}

inline bool removeInventoryItem(State& state, uint16_t index) {
  if (index >= state.inventoryCount) return false;
  for (uint16_t i = index + 1u; i < state.inventoryCount; ++i) state.inventory[i - 1u] = state.inventory[i];
  --state.inventoryCount;
  return true;
}

inline uint8_t equipmentSlot(uint16_t id) {
  const uint16_t type = id < 32u ? id % 12u : (id % kCatalogSize) / 16u % 12u;
  if (type == 5u || type == 10u) return 0;
  if (type == 1u || type == 8u) return 1;
  if (type == 2u || type == 9u) return 2;
  return 3;
}

inline bool useInventoryItem(State& state, uint16_t index) {
  if (index >= state.inventoryCount) return false;
  const uint16_t id = state.inventory[index];
  if (itemIsConsumable(id)) {
    state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 32u));
    state.mana = static_cast<uint16_t>(std::min<uint32_t>(state.maxMana, state.mana + 12u));
    removeInventoryItem(state, index);
    return true;
  }
  state.equipped[equipmentSlot(id)] = id;
  return true;
}

inline const char* npcName(uint8_t id) {
  static const char* names[] = {
      "Mordecai", "Samantha", "Imani", "Louis", "Prepotente", "Odette",
      "Rosetta", "Florin", "Juice Box", "Elle", "Lucia Mar", "Sister Ines",
      "Hekla", "Britney", "Tran", "Dong", "Chris", "Ferdinand",
      "Santiago", "Bautista", "Rend", "Firas", "Bomo", "Orren"};
  return names[id % 24u];
}

inline const char* npcRole(uint8_t id) {
  static const char* roles[] = {
      "Independent guide", "Scavenger quartermaster", "Combat instructor",
      "Trade broker", "Arena champion", "Moon market informant",
      "Signal engineer", "Lucky contract broker", "Sponsor envoy",
      "Healer", "Explorer navigator", "Sanctuary keeper",
      "Fire guild tactician", "Crowd favorite recruiter", "Field medic",
      "Lucky contract trader", "Exfiltration specialist", "Royal envoy",
      "Veteran strategist", "Faction rally captain", "War quartermaster",
      "Bastion engineer", "Creature handler", "Arena judge"};
  return roles[id % 24u];
}

inline void levelUp(State& state) {
  while (state.xp >= static_cast<uint32_t>(state.level * 100u)) {
    state.xp -= state.level * 100u;
    ++state.level;
    state.maxHealth += 12u;
    state.maxMana += 8u;
    state.attributePoints = static_cast<uint8_t>(std::min<uint32_t>(99u, state.attributePoints + 2u));
    state.health = state.maxHealth;
    state.mana = state.maxMana;
  }
}

inline void advanceQuest(State& state) {
  if (state.questProgress < state.questGoal) {
    ++state.questProgress;
    if (state.questProgress == state.questGoal) {
      state.questProgress = 0;
      state.coins += 20u + state.floor * 10u;
      state.reputation = static_cast<uint8_t>(std::min<uint32_t>(100u, state.reputation + 5u));
      state.notoriety = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.notoriety + 25u));
      state.soulCoins += 5u + state.floor;
      if (state.inventoryCount < kInventorySize)
        addItem(state, static_cast<uint16_t>(randomRange(state.seed, 64u)));
      const uint32_t stage = state.floor + state.room / 4u;
      if (stage % 4u == 0u) {
        std::snprintf(state.quest, sizeof(state.quest), "Clear a boss encounter");
        state.questGoal = 1;
      } else if (stage % 3u == 0u) {
        std::snprintf(state.quest, sizeof(state.quest), "Explore dangerous floor routes");
        state.questGoal = 4;
      } else {
        std::snprintf(state.quest, sizeof(state.quest), "Defeat floor monsters");
        state.questGoal = 3;
      }
    }
    return;
  }
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
  state.questProgress = 0;
  state.questGoal = 3;
  std::snprintf(state.quest, sizeof(state.quest), "Defeat 3 floor monsters");
  state.eventId = 0;
  state.routeId = 0;
  state.npcId = 0;
  state.reputation = 50;
  state.attributePoints = 3;
  state.exploredNodes = 0;
  state.battleSkillCooldown = 0;
  state.enemyStunned = 0;
  state.playerShield = 0;
  state.enemyGuard = 0;
  std::snprintf(state.crawlerName, sizeof(state.crawlerName), "%s", "Carl");
  state.originId = 0;
  state.factionId = 0;
  state.appearanceId = 0;
  state.chapter = 1;
  state.notoriety = 0;
  state.soulCoins = 0;
  state.activeSkillId = 0;
  for (uint8_t i = 0; i < 4u; ++i) state.equipped[i] = kUnequippedItem;
  addItem(state, static_cast<uint16_t>(randomRange(state.seed, kCatalogSize)));
  addItem(state, 64u);
}

enum class ExploreChoice : uint8_t { Investigate = 0, Trade = 1, Rest = 2, Challenge = 3, Skill = 4 };

inline void beginEncounter(State& state);

inline const char* eventName(uint8_t eventId) {
  static const char* events[] = {
      "Collapsed Transit Hub", "Sponsor Auction", "NPC Refuge", "Loot Cache",
      "Faction Ambush", "Ancient Flood Channel", "Boss Antechamber",
      "Moon Market", "Butcher's Masquerade", "Bedlam Warcamp", "Feral God Shrine",
      "Inevitable Ruin Vault"};
  return events[eventId % 12u];
}

inline bool explore(State& state, ExploreChoice choice) {
  state.eventId = static_cast<uint8_t>(randomRange(state.seed, 12u));
  state.routeId = static_cast<uint8_t>(randomRange(state.seed, 4u));
  state.npcId = static_cast<uint8_t>(randomRange(state.seed, 24u));
  state.exploredNodes++;

  if (choice == ExploreChoice::Rest) {
    state.health = state.maxHealth;
    state.mana = state.maxMana;
    state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 3u));
    if (state.originId == 1u) state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 15u));
    if (state.factionId == 2u) state.soulCoins += 1u;
    return true;
  }
  if (choice == ExploreChoice::Trade) {
    uint16_t price = static_cast<uint16_t>(6u + state.floor);
    if (state.factionId == 1u && price > 2u) price -= 2u;
    if (state.coins >= price) {
      state.coins -= price;
      state.reputation = static_cast<uint8_t>(std::min<uint32_t>(100u, state.reputation + 4u));
      if (state.inventoryCount < kInventorySize)
        addItem(state, static_cast<uint16_t>(randomRange(state.seed, kCatalogSize)));
    }
    return true;
  }
  if (choice == ExploreChoice::Skill) {
    const uint8_t rank = state.skills[state.activeSkillId];
    if (state.mana >= 8u && rank > 0u) {
      state.mana -= 8u;
      switch (state.activeSkillId) {
        case 0: state.health = state.maxHealth; break;
        case 1: state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 15u)); break;
        case 2: state.mana = state.maxMana; break;
        case 3: state.reputation = static_cast<uint8_t>(std::min<uint32_t>(100u, state.reputation + 3u)); break;
        case 4: if (state.inventoryCount < kInventorySize) addItem(state, static_cast<uint16_t>(randomRange(state.seed, kCatalogSize))); break;
        case 5: state.coins += 12u + rank; break;
        case 6: state.xp += 20u + rank * 3u; levelUp(state); break;
        case 7: state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 20u + rank)); state.mana = static_cast<uint16_t>(std::min<uint32_t>(state.maxMana, state.mana + 12u)); break;
        case 8: state.health = state.maxHealth; state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 5u)); break;
        case 9: state.reputation = static_cast<uint8_t>(std::min<uint32_t>(100u, state.reputation + 6u)); break;
        case 10: state.soulCoins += 2u + rank / 4u; break;
        default: state.notoriety = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.notoriety + 10u)); if (state.inventoryCount < kInventorySize) addItem(state, static_cast<uint16_t>(randomRange(state.seed, 64u))); break;
      }
      state.notoriety = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.notoriety + rank));
    }
    return true;
  }
  if (choice == ExploreChoice::Challenge) {
    beginEncounter(state);
    return true;
  } else {
    const uint32_t reward = 4u + randomRange(state.seed, 10u);
    state.coins += reward;
    state.xp += 12u + state.floor * 2u;
    if (choice == ExploreChoice::Investigate && state.quest[0] != '\0' &&
        std::strstr(state.quest, "Explore dangerous floor routes") != nullptr) {
      advanceQuest(state);
    }
    if (state.inventoryCount < kInventorySize && randomRange(state.seed, 100u) < 70u)
      addItem(state, static_cast<uint16_t>(randomRange(state.seed, kCatalogSize)));
    levelUp(state);
  }
  return false;
}

inline void advanceRoom(State& state) {
  ++state.room;
  if (state.room > 12) {
    state.room = 1;
    ++state.floor;
    state.chapter = static_cast<uint8_t>(std::min<uint32_t>(255u, state.chapter + 1u));
    state.health = static_cast<uint16_t>(state.health + state.maxHealth / 4 < state.maxHealth
                                             ? state.health + state.maxHealth / 4
                                             : state.maxHealth);
  }
}

inline const char* roomName(const State& state) {
  static const char* rooms[] = {"Transit Antechamber", "Moon Market", "Monster Burrow", "Sponsor Arena",
                                "Trap Gallery", "Medic Station", "Faction War Gate", "Merchant Alcove",
                                "Masquerade Hall", "Bedlam Bridge", "Feral Shrine", "Vault of Ruin"};
  return rooms[(state.floor + state.room) % 12];
}

inline const char* enemyName(const State& state) {
  static const char* enemies[] = {
      "Bonebutcher Beetle", "Dungeon Oracle", "Ash Hound", "Prism Goblin",
      "Vault Warden", "Crawler Mimic", "Neon Basilisk", "Sponsor Agent",
      "Masquerade Ghoul", "Feral Godling", "Bedlam Warconstruct", "Ruin Devourer",
      "Faction Hound", "Anarchist Trapper", "Bunker Automat", "Parade Horror"};
  if (state.room == 12u) {
    static const char* bosses[] = {"Klementine the Butcher", "The Bedlam Bride", "Emberus the Judge",
                                   "The Feral God Warden", "Quasar Vault Tyrant"};
    return bosses[(state.floor + state.room / 3u) % 5u];
  }
  return enemies[(state.floor * 3u + state.room) % 16u];
}

inline void beginEncounter(State& state) {
  if (state.enemyMaxHealth != 0) return;
  const bool boss = state.room == 12u;
  state.enemyMaxHealth = static_cast<uint16_t>((boss ? 55 : 18) + state.floor * (boss ? 12 : 7) +
                                               randomRange(state.seed, boss ? 35u : 18u));
  state.enemyHealth = state.enemyMaxHealth;
  state.enemyPower = static_cast<uint16_t>((state.room == 12u ? 9u : 4u) + state.floor * 2 +
                                           randomRange(state.seed, state.room == 12u ? 14u : 8u));
  state.guard = 0;
  state.battleTurn = 1;
  state.combo = 0;
  state.enemyIntent = static_cast<uint8_t>(randomRange(state.seed, 3u));
  state.battleSkillCooldown = 0;
  state.enemyStunned = 0;
  state.playerShield = 0;
  state.enemyGuard = 0;
}

inline bool usePotion(State& state) {
  for (uint16_t i = 0; i < state.inventoryCount; ++i) {
    if (itemIsConsumable(state.inventory[i])) return useInventoryItem(state, i);
  }
  state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 12u));
  return true;
}

enum class BattleAction : uint8_t { Attack = 0, Guard = 1, Arcane = 2, Item = 3, Skill = 4, Escape = 5 };

inline const char* classSkillName(uint8_t classId) {
  static const char* names[] = {"Prism Break", "Battlefield Mend", "Umbral Execution", "System Overload"};
  return names[classId % 4u];
}

inline bool battleTurn(State& state, BattleAction action) {
  if (state.enemyMaxHealth == 0) beginEncounter(state);
  if (action == BattleAction::Escape) {
    if (randomRange(state.seed, 100u) < 35u + state.skills[6] * 2u) {
      state.enemyHealth = 0;
      state.enemyMaxHealth = 0;
      state.guard = 0;
      state.combo = 0;
      state.battleSkillCooldown = 0;
      state.enemyStunned = 0;
      state.playerShield = 0;
      state.enemyGuard = 0;
      state.viewers = static_cast<uint16_t>(std::max<uint32_t>(0u, state.viewers > 2u ? state.viewers - 2u : 0u));
      advanceRoom(state);
      return true;
    }
  }

  uint16_t damage = 0;
  if (action == BattleAction::Attack) {
    damage = static_cast<uint16_t>(10 + state.level * 2 + state.skills[1] * 3 + randomRange(state.seed, 8u));
    if (state.originId == 2u) damage += 3u;
    if (state.factionId == 3u && state.combo >= 2u) damage += 4u;
    for (uint8_t slot = 0; slot < 4u; ++slot) {
      if (state.equipped[slot] < kCatalogSize) damage += static_cast<uint16_t>(itemPower(state.equipped[slot]) / 4u);
    }
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
  } else if (action == BattleAction::Skill) {
    if (state.battleSkillCooldown == 0u && state.mana >= 10u) {
      state.mana -= 10u;
      state.battleSkillCooldown = 3u;
      const uint8_t rank = state.skills[state.activeSkillId];
      switch (state.activeSkillId) {
        case 0: state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 22u + rank * 2u)); state.playerShield += static_cast<uint16_t>(rank); break;
        case 1: damage = static_cast<uint16_t>(18u + state.level * 2u + rank * 4u + randomRange(state.seed, 10u)); break;
        case 2: damage = static_cast<uint16_t>(24u + state.level * 3u + rank * 4u + randomRange(state.seed, 12u)); break;
        case 3: state.guard = static_cast<uint16_t>(18u + rank * 3u); state.mana = static_cast<uint16_t>(std::min<uint32_t>(state.maxMana, state.mana + 8u)); break;
        case 4: state.playerShield += static_cast<uint16_t>(8u + rank * 2u); damage = static_cast<uint16_t>(8u + rank * 2u); break;
        case 5: damage = static_cast<uint16_t>(12u + rank * 5u + randomRange(state.seed, 18u)); break;
        case 6: state.guard = static_cast<uint16_t>(12u + rank * 4u); state.enemyStunned = rank >= 10u ? 1u : 0u; break;
        case 7: state.health = static_cast<uint16_t>(std::min<uint32_t>(state.maxHealth, state.health + 12u + rank)); state.mana = static_cast<uint16_t>(std::min<uint32_t>(state.maxMana, state.mana + 14u)); break;
        case 8: state.playerShield += static_cast<uint16_t>(12u + rank * 3u); break;
        case 9: state.enemyStunned = 1u; state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 5u)); break;
        case 10: state.mana = state.maxMana; damage = static_cast<uint16_t>(10u + rank * 3u); break;
        default: if (randomRange(state.seed, 100u) < 30u + rank * 2u) damage = state.enemyHealth; else damage = static_cast<uint16_t>(10u + rank * 3u);
      }
      state.combo = static_cast<uint8_t>(std::min<uint32_t>(5u, state.combo + 1u));
    }
  }

  if (state.enemyGuard >= damage) {
    state.enemyGuard -= damage;
    damage = 0;
  } else {
    damage -= state.enemyGuard;
    state.enemyGuard = 0;
  }
  if (damage >= state.enemyHealth) state.enemyHealth = 0;
  else state.enemyHealth -= damage;
  if (state.enemyHealth == 0) {
    const uint32_t reward = 10u + state.floor * 5u + randomRange(state.seed, 12u);
    state.coins += reward;
    state.xp += 24u + state.floor * 7u;
    state.kills++;
    state.notoriety = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.notoriety + 8u + state.floor));
    state.soulCoins += 1u + (state.room == 12u ? 10u : 0u);
    advanceQuest(state);
    state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + 4u + randomRange(state.seed, 8u)));
    state.rating = static_cast<uint16_t>(std::min<uint32_t>(100u, state.rating + randomRange(state.seed, 5u)));
    if (state.inventoryCount < kInventorySize) {
      const bool significant = randomRange(state.seed, 100u) < 24u;
      state.inventory[state.inventoryCount++] = static_cast<uint16_t>(significant ? randomRange(state.seed, 64u)
                                                                                 : randomRange(state.seed, kCatalogSize));
    }
    levelUp(state);
    state.enemyMaxHealth = 0;
    state.enemyPower = 0;
    state.guard = 0;
    state.combo = 0;
    state.battleSkillCooldown = 0;
    state.enemyStunned = 0;
    state.playerShield = 0;
    state.enemyGuard = 0;
    advanceRoom(state);
    return true;
  }

  uint16_t incoming = 0;
  if (state.enemyStunned != 0u) {
    state.enemyStunned = 0u;
  } else {
    incoming = state.enemyPower + randomRange(state.seed, 7u);
    if (state.enemyIntent == 0) incoming += 8u;
    if (state.enemyIntent == 2) {
      incoming = incoming / 2u;
      state.enemyGuard = static_cast<uint16_t>(std::min<uint32_t>(50u, state.enemyGuard + 4u + state.floor));
    }
  }
  if (state.playerShield >= incoming) {
    state.playerShield -= incoming;
    incoming = 0;
  } else {
    incoming -= state.playerShield;
    state.playerShield = 0;
  }
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
  if (state.battleSkillCooldown != 0u) --state.battleSkillCooldown;

  if (state.health == 0) {
    ++state.deaths;
    state.health = state.maxHealth;
    state.mana = state.maxMana;
    state.floor = 1;
    state.room = 1;
    state.coins /= 2u;
    state.enemyHealth = 0;
    state.enemyMaxHealth = 0;
    state.battleSkillCooldown = 0;
    state.enemyStunned = 0;
    state.playerShield = 0;
    state.enemyGuard = 0;
  }
  return false;
}

inline bool fight(State& state) {
  const uint32_t roll = randomRange(state.seed, 100);
  const uint16_t enemyPower = static_cast<uint16_t>(5 + state.floor * 3 + randomRange(state.seed, 12));
  const uint16_t playerPower = static_cast<uint16_t>(8 + state.level * 4 + state.skills[1] * 3);
  ++state.kills;
  if (playerPower + roll / 3 >= enemyPower) {
    advanceQuest(state);
    const uint32_t reward = 8u + state.floor * 4u + randomRange(state.seed, 10u);
    state.coins += reward;
    state.xp += 20u + state.floor * 5u;
    state.viewers = static_cast<uint16_t>(std::min<uint32_t>(9999u, state.viewers + randomRange(state.seed, 8u)));
    state.rating = static_cast<uint16_t>(std::min<uint32_t>(100u, state.rating + randomRange(state.seed, 5u)));
    const uint16_t loot = static_cast<uint16_t>(randomRange(state.seed, kCatalogSize));
    if (state.inventoryCount < kInventorySize) addItem(state, loot);
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
