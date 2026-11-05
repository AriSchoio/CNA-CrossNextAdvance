#include "DungeonCrawlActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>

#include "../Shelf.h"
#include "../ui/BackButton.h"
#include "../ui/Dashboard.h"
#include "../ui/GamePanel.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "../../components/SideButtonNavigation.h"

namespace {
namespace fui = freeink::ui;
constexpr int menuRows = 6;
enum Action : freeink::ui::ActionId {
  ActionMenu = 101,
  ActionFight = 102,
  ActionList = 103,
  ActionBack = 104,
  ActionBattleAttack = 105,
  ActionBattleGuard = 106,
  ActionBattleArcane = 107,
  ActionBattleItem = 108,
  ActionBattleEscape = 109,
};
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
  auto rowCount = [&]() -> int {
    switch (view) {
      case View::Menu: return menuRows;
      case View::Inventory: return static_cast<int>(state.inventoryCount);
      case View::Skills: return static_cast<int>(dungeoncrawl::kSkillCount);
      case View::Shop: return 6;
      case View::Dungeon: return 4;
      default: return 1;
    }
  };
  int touchX = -1, touchY = -1;
  if (ready && sidebuttons::poll(interactions, mappedInput, touchX, touchY)) {
    freeink::ui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = touchX;
    input.touchY = touchY;
    const auto event = sidebuttons::route(interactions, input, mappedInput, *this, true);
    if (event.action == ActionMenu) {
      selected = std::clamp<int>(event.value, 0, menuRows - 1);
      switch (selected) {
        case 0: view = View::Dungeon; dungeoncrawl::beginEncounter(state); break;
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
    if (event.action == ActionBack) {
      view = View::Menu;
      selected = 0;
      requestUpdate();
      return;
    }
    if (event.action == ActionFight) {
      dungeoncrawl::beginEncounter(state);
      const bool won = dungeoncrawl::battleTurn(state, dungeoncrawl::BattleAction::Attack);
      std::snprintf(message, sizeof(message), won ? "Victory! Loot acquired and the broadcast gained viewers."
                                                   : "Defeat. The crawl struck you hard.");
      dirty = true;
      save();
      requestUpdate();
      return;
    }
    if (event.action >= ActionBattleAttack && event.action <= ActionBattleEscape) {
      dungeoncrawl::beginEncounter(state);
      const auto action = static_cast<dungeoncrawl::BattleAction>(event.action - ActionBattleAttack);
      const bool won = dungeoncrawl::battleTurn(state, action);
      std::snprintf(message, sizeof(message), won ? "Encounter cleared. Choose the next route carefully."
                                                  : "Turn resolved. Read the enemy intent and adapt.");
      dirty = true;
      save();
      requestUpdate();
      return;
    }
    if (event.action == ActionList) {
      selected = std::max(0, static_cast<int>(event.value));
      if (view == View::Inventory && selected < state.inventoryCount) {
        const uint16_t id = state.inventory[selected];
        state.health = state.health + 12 < state.maxHealth ? state.health + 12 : state.maxHealth;
        state.equipped[selected % 4] = id;
        std::snprintf(message, sizeof(message), "Equipped or used %s.", dungeoncrawl::itemName(id));
        dirty = true;
        save();
      } else if (view == View::Skills && selected < dungeoncrawl::kSkillCount) {
        if (state.coins >= 10 && state.skills[selected] < 20) {
          state.coins -= 10;
          ++state.skills[selected];
          std::snprintf(message, sizeof(message), "%s increased to %u.",
                        dungeoncrawl::skillName(static_cast<uint8_t>(selected)), state.skills[selected]);
          dirty = true;
          save();
        } else {
          std::snprintf(message, sizeof(message), "Need 10 coins; maximum rank is 20.");
        }
      } else if (view == View::Shop) {
        const uint16_t id = static_cast<uint16_t>((state.floor * 37 + selected * 53 + state.room) %
                                                   dungeoncrawl::kCatalogSize);
        const uint16_t price = static_cast<uint16_t>(5 + dungeoncrawl::itemPower(id));
        if (state.coins >= price && dungeoncrawl::addItem(state, id)) {
          state.coins -= price;
          std::snprintf(message, sizeof(message), "Purchased %s.", dungeoncrawl::itemName(id));
          dirty = true;
          save();
        } else {
          std::snprintf(message, sizeof(message), "Not enough coins or the pack is full.");
        }
      }
      requestUpdate();
      return;
    }
  }
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
    const int count = std::max(1, rowCount());
    selected = selected <= 0 ? count - 1 : selected - 1;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    const int count = std::max(1, rowCount());
    selected = (selected + 1) % count;
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (view == View::Menu) {
      switch (selected) {
        case 0: view = View::Dungeon; dungeoncrawl::beginEncounter(state); break;
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
      dungeoncrawl::beginEncounter(state);
      const bool won = dungeoncrawl::battleTurn(state, static_cast<dungeoncrawl::BattleAction>(
                                                         std::min(selected, 3)));
      std::snprintf(message, sizeof(message), won ? "Encounter cleared. Choose the next route carefully."
                                                  : "Turn resolved. Read the enemy intent and adapt.");
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
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = "DCC CRAWL";
  header.back = true;
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
  auto body = screen.body();
  char line[160];

  if (view == View::Menu) {
    static const char* rows[] = {"ENTER THE DUNGEON", "INVENTORY", "SKILLS", "SPONSOR SHOP", "QUEST LOG", "CRAWLER STATUS"};
    static const gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::Book, gamepanel::Mark::Grid,
                                            gamepanel::Mark::Computer, gamepanel::Mark::Flag, gamepanel::Mark::Settings};
    const int gap = 8;
    const int h = std::min(58, std::max(44, (body.height - gap * 5) / 6));
    for (int i = 0; i < menuRows; ++i)
      gamepanel::row(screen, fui::makeRect(body.x, body.y + i * (h + gap), body.width, h), rows[i], ActionMenu, i,
                     marks[i], selected == i);
  } else if (view == View::Dungeon) {
    dungeoncrawl::beginEncounter(state);
    dashboard::frame(screen, body);
    const auto hero = fui::makeRect(body.x + 10, body.y + 10, body.width - 20, body.height * 48 / 100);
    dashboard::frame(screen, hero);
    std::snprintf(line, sizeof(line), "FLOOR %u  /  ROOM %u", state.floor, state.room);
    dashboard::text(screen, fui::makeRect(hero.x + 12, hero.y + 10, hero.width - 24, 34), line, pixelui::kBody,
                    fui::TextAlign::Left);
    dashboard::text(screen, fui::makeRect(hero.x + 12, hero.y + 48, hero.width - 24, 42),
                    dungeoncrawl::roomName(state), pixelui::kTitle, fui::TextAlign::Left);
    dashboard::text(screen, fui::makeRect(hero.x + 12, hero.y + 94, hero.width - 24, 50),
                    dungeoncrawl::enemyName(state), pixelui::kBody, fui::TextAlign::Left);
    std::snprintf(line, sizeof(line), "ENEMY  %u / %u HP   TURN %u", state.enemyHealth, state.enemyMaxHealth,
                  state.battleTurn);
    dashboard::text(screen, fui::makeRect(hero.x + 12, hero.y + 132, hero.width - 24, 28), line,
                    pixelui::kSmall, fui::TextAlign::Left);
    static const char* intents[] = {"INTENT: HEAVY ATTACK", "INTENT: PRECISE ATTACK", "INTENT: RECOVER / WEAKEN"};
    dashboard::text(screen, fui::makeRect(hero.x + 12, hero.bottom() - 38, hero.width - 24, 28),
                    intents[state.enemyIntent % 3], pixelui::kSmall, fui::TextAlign::Left);

    const int cardY = hero.bottom() + 10;
    const int cardW = (body.width - 30) / 3;
    const char* labels[] = {"HP", "VIEWERS", "RATING"};
    uint32_t values[] = {state.health, state.viewers, state.rating};
    for (int i = 0; i < 3; ++i) {
      const auto card = fui::makeRect(body.x + 10 + i * (cardW + 5), cardY, cardW, 70);
      dashboard::frame(screen, card);
      dashboard::text(screen, fui::makeRect(card.x, card.y + 7, card.width, 24), labels[i], pixelui::kSmall);
      std::snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(values[i]));
      dashboard::text(screen, fui::makeRect(card.x, card.y + 34, card.width, 28), line, pixelui::kTitle);
    }
    const int actionY = body.bottom() - 56;
    const int actionGap = 6;
    const int actionW = (body.width - 20 - actionGap * 3) / 4;
    dashboard::button(screen, fui::makeRect(body.x + 10, actionY, actionW, 46),
                      "ATTACK", ActionBattleAttack, 0, selected == 0, nullptr, pixelui::kSmall);
    dashboard::button(screen, fui::makeRect(body.x + 10 + actionW + actionGap, actionY, actionW, 46),
                      "GUARD", ActionBattleGuard, 0, selected == 1, nullptr, pixelui::kSmall);
    dashboard::button(screen, fui::makeRect(body.x + 10 + 2 * (actionW + actionGap), actionY, actionW, 46),
                      "ARCANE", ActionBattleArcane, 0, selected == 2, nullptr, pixelui::kSmall);
    dashboard::button(screen, fui::makeRect(body.x + 10 + 3 * (actionW + actionGap), actionY, actionW, 46),
                      "ITEM", ActionBattleItem, 0, selected == 3, nullptr, pixelui::kSmall);
  } else if (view == View::Inventory) {
    std::snprintf(line, sizeof(line), "PACK  %u / %u", state.inventoryCount, dungeoncrawl::kInventorySize);
    dashboard::text(screen, fui::makeRect(body.x, body.y, body.width, 32), line, pixelui::kBody,
                    fui::TextAlign::Left);
    const int shown = std::min<int>(state.inventoryCount, 7);
    for (int i = 0; i < shown; ++i) {
      char trailing[32];
      std::snprintf(trailing, sizeof(trailing), "PWR %u", dungeoncrawl::itemPower(state.inventory[i]));
      gamepanel::row(screen,
                     fui::makeRect(body.x, body.y + 38 + i * 50, body.width, 46), dungeoncrawl::itemName(state.inventory[i]),
                     ActionList, i, gamepanel::Mark::Book, selected == i, trailing);
    }
  } else if (view == View::Skills) {
    dashboard::text(screen, fui::makeRect(body.x, body.y, body.width, 32), "SKILL TREE  -  10 COINS / RANK",
                    pixelui::kBody, fui::TextAlign::Left);
    const int columns = body.width > 600 ? 2 : 1;
    const int rows = (dungeoncrawl::kSkillCount + columns - 1) / columns;
    const int gap = 6;
    const int cardH = std::min(46, (body.height - 38 - gap * (rows - 1)) / rows);
    const int cardW = (body.width - gap * (columns - 1)) / columns;
    for (int i = 0; i < dungeoncrawl::kSkillCount; ++i) {
      const int col = i / rows, row = i % rows;
      char label[64], rank[24];
      std::snprintf(label, sizeof(label), "%s", dungeoncrawl::skillName(static_cast<uint8_t>(i)));
      std::snprintf(rank, sizeof(rank), "%u / 20", state.skills[i]);
      gamepanel::row(screen,
                     fui::makeRect(body.x + col * (cardW + gap), body.y + 38 + row * (cardH + gap), cardW, cardH),
                     label, ActionList, i, gamepanel::Mark::Grid, selected == i, rank, false);
    }
  } else if (view == View::Shop) {
    dashboard::text(screen, fui::makeRect(body.x, body.y, body.width, 32), "SPONSOR EXCHANGE", pixelui::kBody,
                    fui::TextAlign::Left);
    for (int i = 0; i < 6; ++i) {
      const uint16_t id = static_cast<uint16_t>((state.floor * 37 + i * 53 + state.room) %
                                                 dungeoncrawl::kCatalogSize);
      char price[32];
      std::snprintf(price, sizeof(price), "%u C", 5 + dungeoncrawl::itemPower(id));
      gamepanel::row(screen, fui::makeRect(body.x, body.y + 38 + i * 50, body.width, 46),
                     dungeoncrawl::itemName(id), ActionList, i, gamepanel::Mark::Computer, selected == i, price);
    }
  } else if (view == View::Quests) {
    dashboard::frame(screen, body);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 12, body.width - 24, 42), "ACTIVE CONTRACT",
                    pixelui::kSmall);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 58, body.width - 24, 48), state.quest,
                    pixelui::kTitle);
    std::snprintf(line, sizeof(line), "PROGRESS  %u / %u", state.questProgress, state.questGoal);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 112, body.width - 24, 34), line, pixelui::kBody);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.bottom() - 62, body.width - 24, 42), message,
                    pixelui::kSmall);
  } else if (view == View::Status) {
    dashboard::frame(screen, body);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 12, body.width - 24, 40),
                    dungeoncrawl::className(state.classId), pixelui::kTitle, fui::TextAlign::Left);
    std::snprintf(line, sizeof(line), "LEVEL %u   XP %u / %u   COINS %u", state.level, state.xp,
                  state.level * 100u, state.coins);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 58, body.width - 24, 34), line, pixelui::kBody,
                    fui::TextAlign::Left);
    std::snprintf(line, sizeof(line), "HP %u/%u   MANA %u/%u   KILLS %u   DEATHS %u", state.health,
                  state.maxHealth, state.mana, state.maxMana, state.kills, state.deaths);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 98, body.width - 24, 34), line, pixelui::kSmall,
                    fui::TextAlign::Left);
    std::snprintf(line, sizeof(line), "VIEWERS %u   RATING %u   BOSS FLOOR %u", state.viewers, state.rating,
                  state.floor - state.floor % 5 + 5);
    dashboard::text(screen, fui::makeRect(body.x + 12, body.y + 138, body.width - 24, 34), line, pixelui::kSmall,
                    fui::TextAlign::Left);
  }
  if (saveFailed) dashboard::text(screen, fui::makeRect(body.x, body.bottom() - 24, body.width, 24), "SAVE FAILED",
                                  pixelui::kSmall);
  ready = true;
  toybox::reportOverflow(interactions, "DCC", &renderer);
  renderer.displayBuffer();
}
