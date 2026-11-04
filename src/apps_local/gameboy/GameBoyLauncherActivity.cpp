#include "GameBoyLauncherActivity.h"

#if CROSSINK_GAMEBOY
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "../../activities/home/FileBrowserActivity.h"
#include "../Shelf.h"
#include "GameBoyActivity.h"

std::unique_ptr<Activity> GameBoyLauncherActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<GameBoyLauncherActivity>(renderer, mappedInput);
}

void GameBoyLauncherActivity::onEnter() {
  Activity::onEnter();
  openRomPicker();
}

void GameBoyLauncherActivity::openRomPicker() {
  auto picker =
      makeUniqueNoThrow<FileBrowserActivity>(renderer, mappedInput, "/", FileBrowserActivity::Mode::PickGameBoy);
  if (!picker) {
    LOG_ERR("GAMEBOY", "Could not create ROM picker");
    leaveGame();
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) { onRomPicked(result); });
}

void GameBoyLauncherActivity::onRomPicked(const ActivityResult& result) {
  if (result.isCancelled) {
    leaveGame();
    return;
  }

  const auto* path = std::get_if<FilePathResult>(&result.data);
  if (!path) {
    LOG_ERR("GAMEBOY", "ROM picker returned no file path");
    leaveGame();
    return;
  }

  auto emulator = makeUniqueNoThrow<GameBoyActivity>(renderer, mappedInput, path->path);
  if (!emulator) {
    LOG_ERR("GAMEBOY", "Could not create emulator activity");
    leaveGame();
    return;
  }
  startActivityForResult(std::move(emulator), [this](const ActivityResult&) { leaveGame(); });
}

void GameBoyLauncherActivity::leaveGame() { shelf::leave(renderer, mappedInput); }
#endif
