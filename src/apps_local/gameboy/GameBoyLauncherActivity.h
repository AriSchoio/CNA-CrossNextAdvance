#pragma once

#include <memory>

#include "../../activities/Activity.h"

#ifndef CROSSINK_GAMEBOY
#define CROSSINK_GAMEBOY 0
#endif

#if CROSSINK_GAMEBOY
class GameBoyLauncherActivity final : public Activity {
 public:
  GameBoyLauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameBoy", renderer, mappedInput) {}

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;

 private:
  void openRomPicker();
  void onRomPicked(const ActivityResult& result);
  void leaveGame();
};
#endif
