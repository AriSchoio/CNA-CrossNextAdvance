#include "DungeonCrawlActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_random.h>

#include <cstdio>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"

namespace {
constexpr int menuRows = 6;
}

std::unique_ptr<Activity> DungeonCrawlActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  return makeUniqueNoThrow<DungeonCrawlActivity>(renderer, input);
}

void DungeonCrawlActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  if (!store.load(state)) dungeoncrawl::newGame(state, esp_random());
  requestUpdate();
}

void DungeonCrawlActivity::onExit() {
  save();
  Activity::onExit();
}

void DungeonCrawlActivity::save() {
  if (!dirty) return;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
}

void DungeonCrawlActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::Menu) {
      save();
      shelf::leave(renderer, mappedInput);
    } else {
      view = View::Menu;
      selected = 0;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected = selected <= 0 ? menuRows - 1 : selected - 1;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected = (selected + 1) % menuRows;
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (view == View::Menu) {
      switch (selected) {
        case 0: view = View::Dungeon; break;
        case 1: view = View::Inventory; break;
        case 2: view = View::Skills; break;
        case 3: view = View::Shop; break;
        case 4: view = View::Quests; break;
        case 5: view = View::Status; break;
      }
      selected = 0;
      requestUpdate();
      return;
    }
    if (view == View::Dungeon) {
      const bool won = dungeoncrawl::fight(state);
      std::snprintf(message, sizeof(message), won ? "Victory! Loot acquired and the broadcast gained viewers."
                                                   : "Defeat. The dungeon damaged you, but you survived.");
      dungeoncrawl::advanceRoom(state);
      dirty = true;
      save();
      requestUpdate();
      return;
    }
    if (view == View::Inventory && selected < state.inventoryCount) {
      const uint16_t id = state.inventory[selected];
      if (selected < 4) state.equipped[selected % 4] = id;
      state.health = state.health + 8 < state.maxHealth ? state.health + 8 : state.maxHealth;
      std::snprintf(message, sizeof(message), "Used or equipped %s.", dungeoncrawl::itemName(id));
      dirty = true;
      save();
      requestUpdate();
      return;
    }
    if (view == View::Skills && selected < dungeoncrawl::kSkillCount) {
      if (state.coins >= 10 && state.skills[selected] < 20) {
        state.coins -= 10;
        ++state.skills[selected];
        std::snprintf(message, sizeof(message), "%s increased to level %u.",
                      dungeoncrawl::skillName(static_cast<uint8_t>(selected)), state.skills[selected]);
        dirty = true;
        save();
      } else {
        std::snprintf(message, sizeof(message), "Need 10 coins; skill maximum is 20.");
      }
      requestUpdate();
      return;
    }
    if (view == View::Shop) {
      const uint16_t id = static_cast<uint16_t>((state.floor * 37 + selected * 53 + state.room) %
                                                 dungeoncrawl::kCatalogSize);
      const uint16_t price = static_cast<uint16_t>(5 + dungeoncrawl::itemPower(id));
      if (state.coins >= price && dungeoncrawl::addItem(state, id)) {
        state.coins -= price;
        std::snprintf(message, sizeof(message), "Purchased %s.", dungeoncrawl::itemName(id));
        dirty = true;
        save();
      } else {
        std::snprintf(message, sizeof(message), "Not enough coins or inventory is full.");
      }
      requestUpdate();
      return;
    }
  }

  delay(1);
}

void DungeonCrawlActivity::render(RenderLock&&) {
  renderer.clearScreen();
  int y = 20;
  renderer.drawCenteredText(toybox::kUiFontId, y, "CRAWL: SYSTEM SURVIVAL", true, EpdFontFamily::BOLD);
  y += 36;
  char line[160];

  if (view == View::Menu) {
    static const char* rows[] = {"Enter the dungeon", "Inventory", "Skills", "Sponsor shop", "Quests", "Status"};
    for (int i = 0; i < menuRows; ++i) {
      std::snprintf(line, sizeof(line), "%s%s", selected == i ? "> " : "  ", rows[i]);
      renderer.drawText(toybox::kUiFontId, 20, y + i * 28, line, selected == i);
    }
  } else if (view == View::Dungeon) {
    std::snprintf(line, sizeof(line), "Floor %u  Room %u  HP %u/%u", state.floor, state.room, state.health,
                  state.maxHealth);
    renderer.drawText(toybox::kUiFontId, 20, y, line, true, EpdFontFamily::BOLD);
    renderer.drawText(toybox::kUiFontId, 20, y + 34, "A hostile floor encounter waits.");
    renderer.drawText(toybox::kUiFontId, 20, y + 62, "Press Confirm to fight.");
    renderer.drawText(toybox::kUiFontId, 20, y + 100, message);
  } else if (view == View::Inventory) {
    std::snprintf(line, sizeof(line), "Inventory: %u/%u", state.inventoryCount, dungeoncrawl::kInventorySize);
    renderer.drawText(toybox::kUiFontId, 20, y, line, true, EpdFontFamily::BOLD);
    const int shown = state.inventoryCount < 8 ? state.inventoryCount : 8;
    for (int i = 0; i < shown; ++i) {
      std::snprintf(line, sizeof(line), "%s%s - Power %u", selected == i ? "> " : "  ",
                    dungeoncrawl::itemName(state.inventory[i]), dungeoncrawl::itemPower(state.inventory[i]));
      renderer.drawText(toybox::kUiFontId, 20, y + 34 + i * 24, line, selected == i);
    }
  } else if (view == View::Skills) {
    renderer.drawText(toybox::kUiFontId, 20, y, "Skills - 10 coins each", true, EpdFontFamily::BOLD);
    for (int i = 0; i < 6; ++i) {
      std::snprintf(line, sizeof(line), "%s%s: %u/20", selected == i ? "> " : "  ", dungeoncrawl::skillName(i),
                    state.skills[i]);
      renderer.drawText(toybox::kUiFontId, 20, y + 34 + i * 24, line, selected == i);
    }
  } else if (view == View::Shop) {
    renderer.drawText(toybox::kUiFontId, 20, y, "Sponsor Shop", true, EpdFontFamily::BOLD);
    for (int i = 0; i < 6; ++i) {
      const uint16_t id = static_cast<uint16_t>((state.floor * 37 + i * 53 + state.room) %
                                                 dungeoncrawl::kCatalogSize);
      std::snprintf(line, sizeof(line), "%s%s - %u coins", selected == i ? "> " : "  ",
                    dungeoncrawl::itemName(id), 5 + dungeoncrawl::itemPower(id));
      renderer.drawText(toybox::kUiFontId, 20, y + 34 + i * 24, line, selected == i);
    }
  } else if (view == View::Quests) {
    renderer.drawText(toybox::kUiFontId, 20, y, state.quest, true, EpdFontFamily::BOLD);
    std::snprintf(line, sizeof(line), "Progress: %u/%u", state.questProgress, state.questGoal);
    renderer.drawText(toybox::kUiFontId, 20, y + 34, line);
    renderer.drawText(toybox::kUiFontId, 20, y + 68, message);
  } else if (view == View::Status) {
    std::snprintf(line, sizeof(line), "%s", dungeoncrawl::className(state.classId));
    renderer.drawText(toybox::kUiFontId, 20, y, line, true, EpdFontFamily::BOLD);
    std::snprintf(line, sizeof(line), "Level %u  XP %u/%u  Coins %u", state.level, state.xp, state.level * 100,
                  state.coins);
    renderer.drawText(toybox::kUiFontId, 20, y + 34, line);
    std::snprintf(line, sizeof(line), "HP %u/%u  Mana %u/%u  Kills %u  Deaths %u", state.health, state.maxHealth,
                  state.mana, state.maxMana, state.kills, state.deaths);
    renderer.drawText(toybox::kUiFontId, 20, y + 58, line);
    std::snprintf(line, sizeof(line), "Viewers %u  Rating %u  Catalog %u items", state.viewers, state.rating,
                  dungeoncrawl::kCatalogSize);
    renderer.drawText(toybox::kUiFontId, 20, y + 82, line);
  }
  if (saveFailed)
    renderer.drawText(toybox::kUiFontId, 20, renderer.getDisplayHeight() - 30, "Save failed");
  renderer.displayBuffer();
}
