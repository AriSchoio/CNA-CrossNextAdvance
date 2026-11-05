#pragma once

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "DungeonCrawlCore.h"
#include "DungeonCrawlStore.h"

class DungeonCrawlActivity final : public Activity {
 public:
  DungeonCrawlActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("Dungeon Crawl", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View { Menu, Dungeon, Inventory, Skills, Shop, Quests, Status, Help };
  dungeoncrawl::State state;
  dungeoncrawl::Store store;
  View view = View::Menu;
  int selected = 0;
  bool dirty = false;
  bool saveFailed = false;
   bool ready = false;
   toybox::Interactions interactions;
  char message[160] = "Welcome to the crawl.";
  void save();
};
