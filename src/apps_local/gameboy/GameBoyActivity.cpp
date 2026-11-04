#include "GameBoyActivity.h"

#include "GameBoySettingsStore.h"

#if CROSSINK_GAMEBOY

#include <Arduino.h>
#include <HalDisplay.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_heap_caps.h>
#ifndef SIMULATOR
#include <esp_timer.h>
#endif

#include <FsHelpers.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

#include "CgbDither.h"
#include "fontIds.h"

namespace {
constexpr char TAG[] = "GAMEBOY";
constexpr size_t ROM_HEADER_SIZE = 0x150;
constexpr size_t PEANUT_ROM_BANK_BYTES = 0x4000;
constexpr int TOUCH_TARGET_GAP = 4;

bool isBatteryBackedCartType(const uint8_t type) {
  switch (type) {
    case 0x03:  // MBC1 + RAM + BATTERY
    case 0x06:  // MBC2 + BATTERY
    case 0x09:  // ROM + RAM + BATTERY
    case 0x0F:  // MBC3 + TIMER + BATTERY
    case 0x10:  // MBC3 + TIMER + RAM + BATTERY
    case 0x13:  // MBC3 + RAM + BATTERY
    case 0x1B:  // MBC5 + RAM + BATTERY
    case 0x1E:  // MBC7 + RAM + BATTERY (rejected by Peanut-GB)
      return true;
    default:
      return false;
  }
}

bool hitRect(const int x, const int y, const int rx, const int ry, const int rw, const int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

uint64_t nowMicros() {
#ifdef SIMULATOR
  return static_cast<uint64_t>(micros());
#else
  return static_cast<uint64_t>(esp_timer_get_time());
#endif
}

// Whether panel pixel (x, y) is black for Game Boy shade 0 (white, the
// lightest) to 3 (black). The panel here is black and white only, so the two
// greys are dithered: one dot in four for light grey, three in four for dark.
bool shadeIsBlack(const uint8_t shade, const int x, const int y) {
  switch (shade) {
    case 3:
      return true;
    case 2:
      return ((x ^ y) & 1) == 0 || (x & 1) == 0;
    case 1:
      return (x & 1) == 0 && (y & 1) == 0;
    default:
      return false;
  }
}

}  // namespace

GameBoyActivity::GameBoyActivity(GfxRenderer& rendererIn, MappedInputManager& mappedInputIn, std::string romPathIn)
    : Activity("GameBoy", rendererIn, mappedInputIn), romPath(std::move(romPathIn)) {}

GameBoyActivity::~GameBoyActivity() { cleanup(); }

void GameBoyActivity::onEnter() {
  Activity::onEnter();
  cleanup();

  useWalnutCgb = FsHelpers::checkFileExtension(romPath, ".gbc");
  exitRequested = false;
  saveDirty = false;
  physicalJoypadMask = 0;
  touchJoypadMask = 0;
  activeTouch = TouchControl::None;
  nextFrameUs = 0;
  nextDisplayUs = 0;
  displayDue.store(true, std::memory_order_release);
  error[0] = '\0';
  title[0] = '\0';

  const bool settingsProfileCgb = FsHelpers::checkFileExtension(romPath, ".gbc");
  GameBoyUserSettings userSettings;
  loadGameBoyUserSettings(userSettings, settingsProfileCgb);
  portrait = userSettings.layout == 0;
  touchControlsEnabled = userSettings.touchControls != 0;
  showGameTitle = userSettings.showGameTitle != 0;
  rotateRequested = false;
  renderer.setOrientation(portrait ? GfxRenderer::Portrait : GfxRenderer::LandscapeCounterClockwise);
  setLayout();
  renderer.clearScreen(0xFF);
  chromeKey = CHROME_NONE;

  if (romPath.empty()) {
    showError("No Game Boy ROM selected");
    return;
  }

  if (!loadRom()) {
    return;
  }

  // Select the backend before initEmulator() runs.
  // The header flag also catches CGB-compatible games saved with a .gb extension.
  useWalnutCgb = FsHelpers::checkFileExtension(romPath, ".gbc") || ((rom[0x143] & 0x80U) != 0);

  if (useWalnutCgb != settingsProfileCgb) {
    loadGameBoyUserSettings(userSettings, useWalnutCgb);
    portrait = userSettings.layout == 0;
    touchControlsEnabled = userSettings.touchControls != 0;
    showGameTitle = userSettings.showGameTitle != 0;
    renderer.setOrientation(portrait ? GfxRenderer::Portrait : GfxRenderer::LandscapeCounterClockwise);
    setLayout();
  }

  if (!buildSavePaths() || !initEmulator() || !loadSave()) {
    return;
  }

  // Peanut-GB keeps the RTC registers internally. Seed them from the wall
  // clock on every launch; the emulator advances them during frame execution.
  time_t rawTime = 0;
  time(&rawTime);

  struct tm timeInfo{};
  localtime_r(&rawTime, &timeInfo);

  if (useWalnutCgb) {
    walnut_cgb_set_rtc(cgb, &timeInfo);
  } else {
    gb_set_rtc(gb, &timeInfo);
  }

  char gameTitle[17] = {};

  if (useWalnutCgb) {
    walnut_cgb_get_rom_name(cgb, gameTitle);
  } else {
    gb_get_rom_name(gb, gameTitle);
  }

  std::strncpy(title, gameTitle, sizeof(title) - 1);

  if (title[0] == '\0') {
    std::strncpy(title, useWalnutCgb ? "GAME BOY COLOR" : "GAME BOY", sizeof(title) - 1);
  }

  initialized.store(true, std::memory_order_release);
  applyJoypad();

  requestUpdate(true);

  const uint64_t nowUs = nowMicros();
  nextFrameUs = nowUs;
  nextDisplayUs = nowUs + DISPLAY_PERIOD_US;
  nextAutosaveUs = nowUs + AUTOSAVE_PERIOD_US;
  nextCleanUs = nowUs + CLEAN_PERIOD_US;
  startEmuTask();
  LOG_INF(TAG, "Started %s (%u bytes, save=%u bytes)", title, static_cast<unsigned>(romSize),
          static_cast<unsigned>(cartRamSize));
}

void GameBoyActivity::onExit() {
  stopEmuTask();  // before the save and cleanup touch its state
  if (initialized.load(std::memory_order_acquire)) {
    if (!saveCartRam()) LOG_ERR(TAG, "Save failed during Game Boy exit: %s", savePath.c_str());
  }
  cleanup();
  renderer.setOrientation(GfxRenderer::Portrait);
  Activity::onExit();
}

void GameBoyActivity::loop() {
  handleInput();
  if (exitRequested) {
    mappedInput.swallowCurrentTouch();
    finish();
    return;
  }

  if (!initialized.load(std::memory_order_acquire)) {
    // Global shortcuts are intentionally blocked while the Game Boy activity
    // owns input, so provide a physical Back escape from the error screen too.
    using Button = MappedInputManager::Button;
    if (mappedInput.wasPressed(Button::Back)) exitRequested = true;
#if CROSSINK_APP_CAP_TOUCH
    int tx = 0;
    int ty = 0;
    if (mappedInput.wasScreenTouchDown(tx, ty)) exitRequested = true;
#endif
    if (exitRequested) {
      mappedInput.swallowCurrentTouch();
      finish();
    }
    return;
  }

  if (rotateRequested) {
    rotateRequested = false;
    rotate();
  }
  const uint64_t nowUs = nowMicros();
  if (nowUs >= nextDisplayUs) {
    displayDue.store(true, std::memory_order_release);
    // Do not accumulate an unbounded display backlog when an e-ink refresh or
    // a long emulation stretch takes longer than one target period.
    nextDisplayUs = nowUs + DISPLAY_PERIOD_US;
    requestUpdate();
  }

  // The emulator has its own task; leave CPU 1 to the render task between
  // input polls (this activity skips the main loop's own delay).
  delay(2);
}

void GameBoyActivity::startEmuTask() {
  emuRun.store(true, std::memory_order_release);
  emuStopped.store(false, std::memory_order_release);
  if (xTaskCreatePinnedToCore(&GameBoyActivity::emuTaskEntry, "GameBoyEmu", 12288, this, 1, &emuTask, 0) != pdPASS) {
    LOG_ERR(TAG, "Could not start the emulator task");
    emuTask = nullptr;
    emuRun.store(false, std::memory_order_release);
    emuStopped.store(true, std::memory_order_release);
    showError("Could not start the emulator");
  }
}

// Asks the emulator task to finish its frame and waits for it (bounded).
void GameBoyActivity::stopEmuTask() {
  if (!emuTask) return;
  emuRun.store(false, std::memory_order_release);
  const unsigned long start = millis();
  while (!emuStopped.load(std::memory_order_acquire) && millis() - start < 1000) delay(1);
  if (!emuStopped.load(std::memory_order_acquire)) LOG_ERR(TAG, "Emulator task did not stop in time");
#ifdef SIMULATOR
  vTaskDelete(emuTask);  // the host thread has returned; release its handle
#endif
  emuTask = nullptr;
}

void GameBoyActivity::emuTaskEntry(void* arg) {
  auto* self = static_cast<GameBoyActivity*>(arg);
  self->emuTaskLoop();
  self->emuStopped.store(true, std::memory_order_release);
#ifndef SIMULATOR
  vTaskDelete(nullptr);
#endif
}

void GameBoyActivity::emuTaskLoop() {
  while (emuRun.load(std::memory_order_acquire)) {
    const uint64_t nowUs = nowMicros();
    runEmulation(nowUs);

    if (nowUs >= nextCleanUs) {
      nextCleanUs = nowUs + CLEAN_PERIOD_US;
      fullRefreshDue.store(true, std::memory_order_release);
    }

    if (nowUs >= nextAutosaveUs) {
      nextAutosaveUs = nowUs + AUTOSAVE_PERIOD_US;

      if ((saveDirty || useWalnutCgb) && !saveCartRam()) {
        LOG_ERR(TAG, "Autosave failed: %s", savePath.c_str());
      }
    }

    // Let CPU 0's idle task (and its watchdog) run between frames. While the
    // emulator is behind real time the 1 ms sleep is skipped for up to 15 loops
    // in a row (a yield instead, ~0.4 s at most at the slowest speeds), then
    // taken once: the idle task still runs many times a second, far inside its
    // watchdog period, and a slow game stops losing ~1 ms to every frame.
    if (nowMicros() >= nextFrameUs && behindStreak < 15) {
      ++behindStreak;
      yield();
    } else {
      behindStreak = 0;
      delay(1);
    }
  }
}

void GameBoyActivity::render(RenderLock&&) {
  if (!displayDue.exchange(false, std::memory_order_acq_rel)) return;

  if (!initialized.load(std::memory_order_acquire)) {
    chromeKey = CHROME_NONE;
    renderer.clearScreen(0xFF);
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 32, "GAME BOY", true,
                              EpdFontFamily::BOLD);
    if (error[0]) renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2 + 8, error, true);
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2 + 70, "Touch screen to exit", true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  const bool cleaning = fullRefreshDue.exchange(false, std::memory_order_acq_rel);
  const uint16_t batteryPercent = powerManager.getBatteryPercentage();
  const uint32_t chrome = static_cast<uint32_t>(physicalJoypadMask | touchJoypadMask) |
                          (static_cast<uint32_t>(activeTouch) << 8) | (portrait ? 0x10000u : 0u) |
                          (touchControlsEnabled ? 0x20000u : 0u) | (showGameTitle ? 0x40000u : 0u) |
                          (static_cast<uint32_t>(batteryPercent & 0x7FU) << 20);
  const bool redrawChrome = cleaning || chrome != chromeKey;
  if (redrawChrome) renderer.clearScreen(0xFF);
  drawGameFrame();
  // The picture is in the display's framebuffer now: hand the Game Boy frame
  // back before the e-ink refresh (hundreds of ms), or the emulator, with
  // both of its buffers held, would stand still for the whole refresh.
  releaseRenderedFrame();
  if (redrawChrome) {
    drawControls();
    chromeKey = chrome;
  }
  renderer.displayBuffer(cleaning ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}

// Marks the frame drawGameFrame() copied as consumed and lets the emulator
// write into it again.
void GameBoyActivity::releaseRenderedFrame() {
  const uint8_t claimedFrame = renderFrame.load(std::memory_order_acquire);
  if (claimedFrame > 1) return;
  uint8_t expected = claimedFrame;
  readyFrame.compare_exchange_strong(expected, static_cast<uint8_t>(0xFF), std::memory_order_acq_rel,
                                     std::memory_order_acquire);
  renderFrame.store(0xFF, std::memory_order_release);
}

void GameBoyActivity::setLayout() {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  controlCount = 0;

  if (portrait) {
    // 480x800: the game fills the width on top (y a multiple of 8 for
    // drawGameFramePortrait()), the D-pad and B/A below it as on the handheld,
    // then a row of EXIT, SELECT, START and ROTATE.
    scale = std::max(1, std::min(MAX_GAME_SCALE, screenW / GAMEBOY_WIDTH));
    gameW = GAMEBOY_WIDTH * scale;
    gameH = GAMEBOY_HEIGHT * scale;
    gameX = (screenW - gameW) / 2;
    gameY = 40;
    titleX = 8;
    titleY = 10;

    const int padY = gameY + gameH + 24;
    const int pad = 64;
    const int padX = 14;
    addControl(TouchControl::Up, padX + pad, padY, pad, pad, "U");
    addControl(TouchControl::Left, padX, padY + pad, pad, pad, "L");
    addControl(TouchControl::Right, padX + 2 * pad, padY + pad, pad, pad, "R");
    addControl(TouchControl::Down, padX + pad, padY + 2 * pad, pad, pad, "D");
    const int ab = 80;
    addControl(TouchControl::A, screenW - 20 - ab, padY + 24, ab, ab, "A");
    addControl(TouchControl::B, screenW - 36 - 2 * ab, padY + 24 + ab * 3 / 4, ab, ab, "B");

    const int rowH = 52;
    const int rowY = screenH - 12 - rowH;
    const int rowW = (screenW - 8 - 3 * TOUCH_TARGET_GAP) / 4;
    const TouchControl row[] = {TouchControl::Exit, TouchControl::Select, TouchControl::Start, TouchControl::Rotate};
    const char* rowLabels[] = {"EXIT", "SEL", "START", "ROTATE"};
    for (int i = 0; i < 4; ++i) {
      addControl(row[i], 4 + i * (rowW + TOUCH_TARGET_GAP), rowY, rowW, rowH, rowLabels[i]);
    }
    return;
  }

  // 800x480: the game in the middle, the D-pad and ROTATE on the left, B/A,
  // SELECT/START and EXIT on the right.
  const int availableH = std::max(GAMEBOY_HEIGHT, screenH - 48);
  scale = std::min({MAX_GAME_SCALE, std::max(GAMEBOY_WIDTH, screenW) / GAMEBOY_WIDTH, availableH / GAMEBOY_HEIGHT});
  scale = std::max(1, scale);
  gameW = GAMEBOY_WIDTH * scale;
  gameH = GAMEBOY_HEIGHT * scale;
  gameX = (screenW - gameW) / 2;
  gameY = std::max(0, (screenH - gameH) / 2);
  titleX = 6;
  titleY = 18;

  const int leftW = gameX;
  const int panelY = gameY + 120;
  const int centerX = leftW / 2;
  addControl(TouchControl::Up, centerX - 35, panelY, 70, 58, "U");
  addControl(TouchControl::Down, centerX - 35, panelY + 140, 70, 58, "D");
  addControl(TouchControl::Left, 10, panelY + 58, 60, 82, "L");
  addControl(TouchControl::Right, leftW - 70, panelY + 58, 60, 82, "R");
  addControl(TouchControl::Rotate, 4, gameY + gameH - 52, leftW - 8, 48, "ROTATE");

  const int rightX = gameX + gameW;
  const int rightW = screenW - rightX;
  const int buttonW = std::min(70, std::max(48, (rightW - TOUCH_TARGET_GAP) / 2));
  const int firstX = rightX + 4;
  const int secondX = firstX + buttonW + TOUCH_TARGET_GAP;
  addControl(TouchControl::B, firstX, panelY, buttonW, 70, "B");
  addControl(TouchControl::A, secondX, panelY, buttonW, 70, "A");
  addControl(TouchControl::Select, firstX, gameY + 275, buttonW, 52, "SEL");
  addControl(TouchControl::Start, secondX, gameY + 275, buttonW, 52, "START");
  addControl(TouchControl::Exit, rightX + 4, gameY + gameH - 52, rightW - 8, 48, "EXIT");
}

void GameBoyActivity::addControl(const TouchControl control, const int x, const int y, const int w, const int h,
                                 const char* label) {
  if (controlCount >= MAX_CONTROLS) return;
  controls[controlCount++] = {control, x, y, w, h, label};
}

// Hands the refresh preset to the display driver (under the render lock: a
// refresh may be running) and asks for a cleaning refresh next.
void GameBoyActivity::rotate() {
  // The buttons move, so the finger on ROTATE must not press whatever lands
  // under it.
  mappedInput.swallowCurrentTouch();
  activeTouch = TouchControl::None;
  touchJoypadMask = 0;
  applyJoypad();
  {
    RenderLock lock(*this);
    portrait = !portrait;
    renderer.setOrientation(portrait ? GfxRenderer::Portrait : GfxRenderer::LandscapeCounterClockwise);
    setLayout();
  }
  GameBoyUserSettings settings;
  loadGameBoyUserSettings(settings, useWalnutCgb);
  settings.layout = portrait ? 0 : 1;
  saveGameBoyUserSettings(settings, useWalnutCgb);
  fullRefreshDue.store(true, std::memory_order_release);
  displayDue.store(true, std::memory_order_release);
  requestUpdate();
}

size_t GameBoyActivity::romBankCountFromCode(const uint8_t code) {
  // Peanut-GB v1.3.0's internal ROM-size table covers codes 0..8.
  if (code > 8) return 0;
  return static_cast<size_t>(2) << code;
}

bool GameBoyActivity::isBatteryBackedCart(const uint8_t type) { return isBatteryBackedCartType(type); }

bool GameBoyActivity::loadRom() {
  HalFile file;
  if (!Storage.openFileForRead(TAG, romPath, file)) {
    showError("Unable to open Game Boy ROM");
    return false;
  }

  const uint64_t fileSize = file.fileSize64();
  if (fileSize < ROM_HEADER_SIZE || fileSize > static_cast<uint64_t>(SIZE_MAX)) {
    file.close();
    showError("ROM size is invalid");
    return false;
  }

  uint8_t romSizeCode = 0xFF;
  if (!file.seekSet(0x148) || file.read(&romSizeCode, 1) != 1) {
    file.close();
    showError("ROM header could not be read");
    return false;
  }

  const size_t bankCount = romBankCountFromCode(romSizeCode);
  if (bankCount == 0) {
    file.close();
    showError("Unsupported ROM size");
    return false;
  }

  const size_t expectedSize = bankCount * PEANUT_ROM_BANK_BYTES;
  if (fileSize < expectedSize) {
    file.close();
    showError("ROM file is incomplete");
    return false;
  }

  // Allocate the cartridge size declared by the ROM header, not any trailing
  // padding some dump tools append to the file. This keeps PSRAM usage bounded
  // by the actual Game Boy cartridge image.
  rom = static_cast<uint8_t*>(heap_caps_malloc(expectedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rom) {
    file.close();
    showError("Not enough PSRAM for ROM");
    return false;
  }

  if (!file.seekSet(0)) {
    file.close();
    showError("ROM seek failed");
    return false;
  }

  size_t offset = 0;
  const size_t totalSize = expectedSize;
  while (offset < totalSize) {
    const size_t chunk = std::min<size_t>(16384, totalSize - offset);
    const int got = file.read(rom + offset, chunk);
    if (got <= 0) {
      file.close();
      showError("ROM read failed");
      return false;
    }
    offset += static_cast<size_t>(got);
  }
  file.close();
  romSize = totalSize;

  if (rom[0x149] > 5) {
    showError("Unsupported cartridge RAM");
    return false;
  }

  return true;
}

bool GameBoyActivity::buildSavePaths() {
  savePath = romPath;
  const size_t slash = savePath.find_last_of('/');
  const size_t dot = savePath.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    savePath += ".sav";
  else
    savePath.replace(dot, std::string::npos, ".sav");

  tempSavePath = savePath + ".tmp";
  backupSavePath = savePath + ".bak";
  return savePath.size() < 512 && tempSavePath.size() < 512 && backupSavePath.size() < 512;
}

bool GameBoyActivity::initEmulator() {
  if (useWalnutCgb) {
    int result = WALNUT_CGB_INIT_OUT_OF_MEMORY;

    cgb = walnut_cgb_create(rom, romSize, &GameBoyActivity::walnutLcdDrawLine, &GameBoyActivity::walnutFatalError, this,
                            &result);

    if (!cgb) {
      char message[80];
      snprintf(message, sizeof(message), "Walnut-CGB init failed (%d)", result);
      showError(message);
      return false;
    }

    // The CGB context owns this cartridge RAM.
    cartRamSize = walnut_cgb_save_size(cgb);
    cartRam = walnut_cgb_save_ram(cgb);

    // The emulator state goes in internal RAM when plenty is free (it is
    // returned when the game ends), otherwise in PSRAM; say which, with what
    // is left, so a speed log can be read against it.
    LOG_INF(TAG, "Walnut-CGB state in %s; internal heap free %u, largest block %u",
            walnut_cgb_state_in_internal_ram(cgb) ? "internal RAM" : "PSRAM",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    lastConsumeUs32.store(static_cast<uint32_t>(nowMicros()), std::memory_order_relaxed);
    consumePeriodUs32.store(static_cast<uint32_t>(DISPLAY_PERIOD_US), std::memory_order_relaxed);
  } else {
    // Preserve the original Peanut-GB initialization for .gb ROMs.
    gb = static_cast<gb_s*>(calloc(1, sizeof(gb_s)));
    if (!gb) {
      showError("Not enough memory for emulator");
      return false;
    }

    const enum gb_init_error_e initResult = gb_init(gb, &GameBoyActivity::romRead, &GameBoyActivity::cartRamRead,
                                                    &GameBoyActivity::cartRamWrite, &GameBoyActivity::fatalError, this);

    if (initResult != GB_INIT_NO_ERROR) {
      showError(initErrorString(static_cast<int>(initResult)));
      return false;
    }

    size_t saveSize = 0;
    if (gb_get_save_size_s(gb, &saveSize) < 0) {
      showError("Unknown cartridge RAM size");
      return false;
    }

    if (saveSize > 0) {
      cartRam = static_cast<uint8_t*>(heap_caps_malloc(saveSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

      if (!cartRam) {
        showError("Not enough PSRAM for save RAM");
        return false;
      }

      memset(cartRam, 0, saveSize);
      cartRamSize = saveSize;
    }
  }

  // Both cores use the same double-buffered rendering path.
  frameBuffers[0] = static_cast<uint8_t*>(heap_caps_malloc(EMULATOR_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  frameBuffers[1] = static_cast<uint8_t*>(heap_caps_malloc(EMULATOR_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (!frameBuffers[0] || !frameBuffers[1]) {
    showError("Not enough PSRAM for Game Boy video");
    return false;
  }

  memset(frameBuffers[0], 0, EMULATOR_FRAME_BYTES);
  memset(frameBuffers[1], 0, EMULATOR_FRAME_BYTES);

  writeFrame = 0;
  readyFrame.store(0xFF, std::memory_order_release);
  renderFrame.store(0xFF, std::memory_order_release);

  // Walnut-CGB installs its line callback in walnut_cgb_create().
  if (!useWalnutCgb) {
    gb_init_lcd(gb, &GameBoyActivity::lcdDrawLine);
  }

  return true;
}

bool GameBoyActivity::loadSave() {
  if (cartRamSize == 0 || !cartRam || (!useWalnutCgb && !isBatteryBackedCart(rom[0x147]))) {
    return true;
  }

  std::memset(cartRam, 0, cartRamSize);
  HalFile file;
  if (!Storage.openFileForRead(TAG, savePath, file)) return true;  // New save; normal case.

  size_t offset = 0;
  while (offset < cartRamSize) {
    const size_t want = cartRamSize - offset;
    const int got = file.read(cartRam + offset, want);
    if (got <= 0) break;
    offset += static_cast<size_t>(got);
  }
  file.close();
  LOG_INF(TAG, "Loaded save: %u/%u bytes", static_cast<unsigned>(offset), static_cast<unsigned>(cartRamSize));
  return true;
}

bool GameBoyActivity::saveCartRam() {
  if ((!saveDirty && !useWalnutCgb) || cartRamSize == 0 || !cartRam || !rom ||
      (!useWalnutCgb && !isBatteryBackedCart(rom[0x147]))) {
    return true;
  }

  HalFile file;
  if (!Storage.openFileForWrite(TAG, tempSavePath, file)) {
    LOG_ERR(TAG, "Unable to open save temp file: %s", tempSavePath.c_str());
    return false;
  }

  const size_t written = file.write(cartRam, cartRamSize);
  const bool fullyWritten = written == cartRamSize;
  file.flush();
  const bool closed = file.close();
  if (!fullyWritten || !closed) {
    Storage.remove(tempSavePath.c_str());
    LOG_ERR(TAG, "Save write failed: wrote=%u expected=%u", static_cast<unsigned>(written),
            static_cast<unsigned>(cartRamSize));
    return false;
  }

  const bool hadExisting = Storage.exists(savePath.c_str());
  if (hadExisting) {
    Storage.remove(backupSavePath.c_str());
    if (!Storage.rename(savePath.c_str(), backupSavePath.c_str())) {
      Storage.remove(tempSavePath.c_str());
      LOG_ERR(TAG, "Could not stage previous save before commit");
      return false;
    }
  }

  if (!Storage.rename(tempSavePath.c_str(), savePath.c_str())) {
    if (hadExisting) Storage.rename(backupSavePath.c_str(), savePath.c_str());
    Storage.remove(tempSavePath.c_str());
    LOG_ERR(TAG, "Could not commit save: %s", savePath.c_str());
    return false;
  }

  if (hadExisting) Storage.remove(backupSavePath.c_str());
  saveDirty = false;
  LOG_INF(TAG, "Saved: %s", savePath.c_str());
  return true;
}

void GameBoyActivity::cleanup() {
  if (cgb) {
    walnut_cgb_destroy(cgb);
    cgb = nullptr;

    // This pointer belonged to the Walnut-CGB context.
    cartRam = nullptr;
    cartRamSize = 0;
  }

  if (gb) {
    free(gb);
    gb = nullptr;
  }

  if (rom) {
    heap_caps_free(rom);
    rom = nullptr;
  }

  if (cartRam) {
    heap_caps_free(cartRam);
    cartRam = nullptr;
  }
  for (auto*& buffer : frameBuffers) {
    if (buffer) heap_caps_free(buffer);
    buffer = nullptr;
  }
  writeFrame = 0;
  readyFrame.store(0xFF, std::memory_order_release);
  renderFrame.store(0xFF, std::memory_order_release);
  romSize = 0;
  cartRamSize = 0;
  physicalJoypadMask = 0;
  touchJoypadMask = 0;
  activeTouch = TouchControl::None;
  saveDirty = false;
}

void GameBoyActivity::showError(const char* message) {
  std::strncpy(error, message ? message : "Game Boy error", sizeof(error) - 1);
  error[sizeof(error) - 1] = '\0';
  initialized.store(false, std::memory_order_release);
  displayDue.store(true, std::memory_order_release);
  requestUpdate();
}

void GameBoyActivity::handleInput() {
  using Button = MappedInputManager::Button;
  const Button buttons[] = {Button::Up,      Button::Down, Button::Left, Button::Right,
                            Button::Confirm, Button::Back, Button::Power};
  uint8_t nextPhysical = 0;
  for (const Button button : buttons) {
    if (mappedInput.isPressed(button)) nextPhysical |= joypadBitForButton(button);
  }
  if (nextPhysical != physicalJoypadMask) {
    physicalJoypadMask = nextPhysical;
    applyJoypad();
  }

#if CROSSINK_APP_CAP_TOUCH
  if (touchControlsEnabled) {
    int x = 0;
    int y = 0;
    if (mappedInput.wasScreenTouchDown(x, y)) updateTouch(x, y);
    if (mappedInput.isScreenTouchHeld(x, y)) {
      updateTouch(x, y);
    } else if (mappedInput.wasScreenTouchReleased()) {
      activeTouch = TouchControl::None;
      touchJoypadMask = 0;
      applyJoypad();
    }
  } else {
    activeTouch = TouchControl::None;
    touchJoypadMask = 0;
    applyJoypad();
  }
#endif
}

void GameBoyActivity::updateTouch(const int x, const int y) {
  if (!touchControlsEnabled) return;
  const TouchControl control = hitTestTouch(x, y);
  if (control == TouchControl::Exit) {
    exitRequested = true;
    return;
  }
  if (control == TouchControl::Rotate) {
    rotateRequested = true;
    return;
  }
  if (control == activeTouch) return;
  activeTouch = control;
  touchJoypadMask = joypadBitForTouch(control);
  applyJoypad();
}

GameBoyActivity::TouchControl GameBoyActivity::hitTestTouch(const int x, const int y) const {
  for (size_t i = 0; i < controlCount; ++i) {
    const ControlButton& c = controls[i];
    if (hitRect(x, y, c.x, c.y, c.w, c.h)) return c.control;
  }
  return TouchControl::None;
}

uint8_t GameBoyActivity::joypadBitForTouch(const TouchControl control) {
  switch (control) {
    case TouchControl::Up:
      return JOYPAD_UP;
    case TouchControl::Down:
      return JOYPAD_DOWN;
    case TouchControl::Left:
      return JOYPAD_LEFT;
    case TouchControl::Right:
      return JOYPAD_RIGHT;
    case TouchControl::A:
      return JOYPAD_A;
    case TouchControl::B:
      return JOYPAD_B;
    case TouchControl::Select:
      return JOYPAD_SELECT;
    case TouchControl::Start:
      return JOYPAD_START;
    default:
      return 0;
  }
}

uint8_t GameBoyActivity::joypadBitForButton(const MappedInputManager::Button button) {
  switch (button) {
    case MappedInputManager::Button::Up:
      return JOYPAD_UP;
    case MappedInputManager::Button::Down:
      return JOYPAD_DOWN;
    case MappedInputManager::Button::Left:
      return JOYPAD_LEFT;
    case MappedInputManager::Button::Right:
      return JOYPAD_RIGHT;
    case MappedInputManager::Button::Confirm:
      return JOYPAD_START;
    case MappedInputManager::Button::Back:
      return JOYPAD_SELECT;
    case MappedInputManager::Button::Power:
      return JOYPAD_A;
    default:
      return 0;
  }
}

void GameBoyActivity::applyJoypad() {
  const uint8_t activeLow = static_cast<uint8_t>(~(physicalJoypadMask | touchJoypadMask));

  if (useWalnutCgb && cgb) {
    walnut_cgb_set_joypad(cgb, activeLow);
  } else if (gb) {
    gb->direct.joypad = activeLow;
  }
}

void GameBoyActivity::runEmulation(const uint64_t nowUs) {
  if (!gb && !cgb) return;

  if (nextFrameUs == 0) {
    nextFrameUs = nowUs;
  }

  uint8_t ran = 0;
  while (nowUs >= nextFrameUs && ran < MAX_CATCH_UP_FRAMES) {
    // A frame whose scanlines are not composed writes to neither buffer, so it
    // needs no free one and is never published (see shouldDrawFrame()).
    const bool draw = shouldDrawFrame(nowUs);

    if (draw) {
      // Do not start a frame in either buffer while the display task owns it,
      // and do not overwrite the most recently completed frame before it has
      // been consumed. With two buffers this gives a simple, lock-free handoff.
      const uint8_t pending = readyFrame.load(std::memory_order_acquire);
      const uint8_t rendering = renderFrame.load(std::memory_order_acquire);
      if (writeFrame == pending || writeFrame == rendering) {
        break;
      }
    }

    if (useWalnutCgb) {
      if (!cgb) {
        LOG_ERR(TAG, "Walnut-CGB selected but context is null");
        break;
      }

      walnut_cgb_run_frame_ex(cgb, draw ? 1 : 0);
    } else {
      if (!gb) {
        LOG_ERR(TAG, "Peanut-GB selected but context is null");
        break;
      }

      gb_run_frame(gb);
    }

    if (draw) {
      publishCompletedFrame();
    }
    nextFrameUs += EMULATION_FRAME_PERIOD_US;
    ++ran;
  }

  if (ran == MAX_CATCH_UP_FRAMES && nowUs >= nextFrameUs) nextFrameUs = nowUs + EMULATION_FRAME_PERIOD_US;
}

// Whether the frame about to be emulated should have its scanlines composed.
// Always for Peanut-GB (the .gb path is unchanged). For Walnut-CGB: when no
// finished picture is waiting for the display (so there is always something to
// show), and from DRAW_LEAD_US before the display is expected to take the next
// one, judged from how often it has been taking them. When unsure the answer is
// yes, which is what every frame did before.
bool GameBoyActivity::shouldDrawFrame(const uint64_t nowUs) const {
  if (!useWalnutCgb) return true;
  if (readyFrame.load(std::memory_order_acquire) == 0xFF) return true;
  const uint32_t sinceConsume = static_cast<uint32_t>(nowUs) - lastConsumeUs32.load(std::memory_order_relaxed);
  return sinceConsume + DRAW_LEAD_US >= consumePeriodUs32.load(std::memory_order_relaxed);
}

// Render task: the display just took a picture. Tracks the interval between
// pictures (a moving average, ignoring pauses such as a menu or an exit).
void GameBoyActivity::noteFrameConsumed() {
  const uint32_t nowUs32 = static_cast<uint32_t>(nowMicros());
  const uint32_t interval = nowUs32 - lastConsumeUs32.load(std::memory_order_relaxed);
  if (interval >= MIN_CONSUME_PERIOD_US && interval <= MAX_CONSUME_PERIOD_US) {
    const uint32_t period = consumePeriodUs32.load(std::memory_order_relaxed);
    consumePeriodUs32.store((period * 3U + interval) / 4U, std::memory_order_relaxed);
  }
  lastConsumeUs32.store(nowUs32, std::memory_order_relaxed);
}

void GameBoyActivity::publishCompletedFrame() {
  readyFrame.store(writeFrame, std::memory_order_release);
  writeFrame ^= 1u;
}

void GameBoyActivity::drawGameFrame() {
  const uint8_t frameIndex = readyFrame.load(std::memory_order_acquire);
  if (frameIndex > 1) return;

  const uint8_t* source = frameBuffers[frameIndex];
  uint8_t* framebuffer = renderer.getFrameBuffer();
  if (!source || !framebuffer) return;

  // Claim the exact source buffer before reading it. The emulation task checks
  // this ownership marker before starting its next frame. Keep the claim until
  // the display refresh has consumed the CrossInk framebuffer.
  renderFrame.store(frameIndex, std::memory_order_release);
  if (useWalnutCgb) noteFrameConsumed();

  const bool drawn =
      portrait ? drawGameFramePortrait(source, framebuffer) : drawGameFrameLandscape(source, framebuffer);
  if (!drawn) renderFrame.store(0xFF, std::memory_order_release);
}

namespace {

// Builds one panel row of the game a byte at a time: `count` game pixels read
// from `src` every `step` bytes, each `scale` panel pixels long. A game pixel
// becomes the bits patterns[shade][first panel pixel's parity] (1 = black,
// most significant first), so the row is about count table lookups.
constexpr int MAX_GAME_ROW_BYTES = 64;
void buildGameRow(uint8_t* out, const uint8_t* src, const int step, const int count, const int scale,
                  const uint8_t (&patterns)[4][2]) {
  uint32_t acc = 0;
  int bits = 0;
  for (int g = 0; g < count; ++g) {
    acc = (acc << scale) | patterns[src[g * step] & 0x03][(g * scale) & 1];
    bits += scale;
    if (bits >= 8) {
      bits -= 8;
      *out++ = static_cast<uint8_t>(~(acc >> bits));  // the framebuffer's 1 is white
      acc &= (1u << bits) - 1u;
    }
  }
}

// patterns[fixedParity][shade][alongParity] for buildGameRow: the dither of
// shadeIsBlack() for a panel row whose fixed coordinate has parity
// fixedParity. alongIsX says whether the row runs along the dither's x.
void gamePatterns(uint8_t (&patterns)[2][4][2], const int scale, const bool alongIsX) {
  for (int fixed = 0; fixed < 2; ++fixed) {
    for (int shade = 0; shade < 4; ++shade) {
      for (int along0 = 0; along0 < 2; ++along0) {
        uint8_t pattern = 0;
        for (int k = 0; k < scale; ++k) {
          const int along = along0 + k;
          const bool black = alongIsX ? shadeIsBlack(shade, along, fixed) : shadeIsBlack(shade, fixed, along);
          pattern = static_cast<uint8_t>((pattern << 1) | (black ? 1 : 0));
        }
        patterns[fixed][shade][along0] = pattern;
      }
    }
  }
}

}  // namespace

bool GameBoyActivity::drawGameFrameLandscape(const uint8_t* source, uint8_t* framebuffer) {
  // LandscapeCounterClockwise is the native X4 Pro framebuffer orientation,
  // so logical (x,y) maps directly to physical (x,y). Peanut-GB's four shades
  // become black, two dithered greys and white (shadeIsBlack()). A game line's
  // panel rows differ only by their y parity: two are built, the rest copied.
  const int panelBytes = renderer.getDisplayWidthBytes();
  const int screenW = renderer.getDisplayWidth();
  const int rowBytes = gameW / 8;
  if (panelBytes <= 0 || screenW <= 0 || gameX < 0 || gameX % 8 != 0 || gameX + gameW > screenW ||
      rowBytes > MAX_GAME_ROW_BYTES) {
    return false;
  }
  if (useWalnutCgb) {
    return cgbdither::cgbDitherFrame(source, framebuffer, gameX, gameY, scale, panelBytes, renderer.getDisplayHeight(),
                                     false);
  }
  uint8_t patterns[2][4][2];
  gamePatterns(patterns, scale, /*alongIsX=*/true);
  uint8_t rows[2][MAX_GAME_ROW_BYTES];
  for (int gy = 0; gy < GAMEBOY_HEIGHT; ++gy) {
    const uint8_t* line = source + gy * GAMEBOY_WIDTH;
    for (int parity = 0; parity < 2; ++parity) {
      buildGameRow(rows[parity], line, 1, GAMEBOY_WIDTH, scale, patterns[parity]);
    }
    for (int sy = 0; sy < scale; ++sy) {
      const int panelY = gy * scale + sy;
      uint8_t* out = framebuffer + static_cast<size_t>(gameY + panelY) * panelBytes + gameX / 8;
      std::memcpy(out, rows[panelY & 1], static_cast<size_t>(rowBytes));
    }
  }
  return true;
}

bool GameBoyActivity::drawGameFramePortrait(const uint8_t* source, uint8_t* framebuffer) {
  // Portrait turns the panel a quarter: logical (x, y) is panel
  // (y, panelHeight - 1 - x), so each game column is one panel row with the
  // game's lines running along it. A column's panel rows differ only by their
  // x parity: two are built, the rest copied.
  const int panelBytes = renderer.getDisplayWidthBytes();
  const int panelW = renderer.getDisplayWidth();
  const int panelH = renderer.getDisplayHeight();
  const int rowBytes = gameH / 8;
  if (panelBytes <= 0 || gameY % 8 != 0 || gameH % 8 != 0 || gameX < 0 || gameX + gameW > panelH ||
      gameY + gameH > panelW || rowBytes > MAX_GAME_ROW_BYTES) {
    return false;
  }
  if (useWalnutCgb) {
    return cgbdither::cgbDitherFrame(source, framebuffer, gameX, gameY, scale, panelBytes, panelH, true);
  }
  uint8_t patterns[2][4][2];
  gamePatterns(patterns, scale, /*alongIsX=*/false);
  uint8_t rows[2][MAX_GAME_ROW_BYTES];
  for (int gx = 0; gx < GAMEBOY_WIDTH; ++gx) {
    for (int parity = 0; parity < 2; ++parity) {
      buildGameRow(rows[parity], source + gx, GAMEBOY_WIDTH, GAMEBOY_HEIGHT, scale, patterns[parity]);
    }
    for (int sx = 0; sx < scale; ++sx) {
      const int dstX = gx * scale + sx;
      uint8_t* out = framebuffer + static_cast<size_t>(panelH - 1 - (gameX + dstX)) * panelBytes + gameY / 8;
      std::memcpy(out, rows[dstX & 1], static_cast<size_t>(rowBytes));
    }
  }
  return true;
}

void GameBoyActivity::drawControls() {
  const uint8_t currentMask = static_cast<uint8_t>(physicalJoypadMask | touchJoypadMask);

  renderer.drawRect(gameX - 1, gameY - 1, gameW + 2, gameH + 2, true);
  char batteryLabel[8];
  std::snprintf(batteryLabel, sizeof(batteryLabel), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  const int batteryWidth = renderer.getTextWidth(UI_10_FONT_ID, batteryLabel);
  const int batteryX = std::max(0, renderer.getScreenWidth() - batteryWidth - 8);
  if (showGameTitle) {
    const char* gameTitle = title[0] ? title : "GAME BOY";
    const int titleAvailable = std::max(1, batteryX - titleX - 8);
    const std::string visibleTitle =
        renderer.truncatedText(UI_10_FONT_ID, gameTitle, titleAvailable, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, titleX, titleY, visibleTitle.c_str(), true, EpdFontFamily::BOLD);
  }
  renderer.drawText(UI_10_FONT_ID, batteryX, titleY, batteryLabel, true, EpdFontFamily::REGULAR);
  if (!touchControlsEnabled) return;
  for (size_t i = 0; i < controlCount; ++i) {
    const ControlButton& c = controls[i];
    const uint8_t bit = joypadBitForTouch(c.control);
    const bool active = bit != 0 ? (currentMask & bit) != 0 : activeTouch == c.control;
    renderer.fillRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, active);
    renderer.drawRect(c.x, c.y, c.w, c.h, true);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, c.label);
    renderer.drawText(UI_10_FONT_ID, c.x + (c.w - tw) / 2, c.y + c.h / 2 - 7, c.label, !active, EpdFontFamily::BOLD);
  }
}

uint8_t GameBoyActivity::romRead(gb_s* state, const uint_fast32_t addr) {
  auto* self = state ? static_cast<GameBoyActivity*>(state->direct.priv) : nullptr;
  if (!self || !self->rom || addr >= self->romSize) return 0xFF;
  return self->rom[addr];
}

uint8_t GameBoyActivity::cartRamRead(gb_s* state, const uint_fast32_t addr) {
  auto* self = state ? static_cast<GameBoyActivity*>(state->direct.priv) : nullptr;
  if (!self || !self->cartRam || addr >= self->cartRamSize) return 0xFF;
  return self->cartRam[addr];
}

void GameBoyActivity::cartRamWrite(gb_s* state, const uint_fast32_t addr, const uint8_t value) {
  auto* self = state ? static_cast<GameBoyActivity*>(state->direct.priv) : nullptr;
  if (!self || !self->cartRam || addr >= self->cartRamSize) return;
  if (self->cartRam[addr] != value) self->saveDirty = true;
  self->cartRam[addr] = value;
}

void GameBoyActivity::lcdDrawLine(gb_s* state, const uint8_t* pixels, const uint_fast8_t line) {
  auto* self = state ? static_cast<GameBoyActivity*>(state->direct.priv) : nullptr;

  if (self) {
    self->writeGameBoyLine(pixels, line);
  }
}  // Important: close lcdDrawLine() here.

void GameBoyActivity::walnutLcdDrawLine(void* user, const uint8_t* pixels, const uint_fast8_t line) {
  auto* self = static_cast<GameBoyActivity*>(user);

  if (self) {
    self->writeGameBoyLine(pixels, line);
  }
}

void GameBoyActivity::walnutFatalError(void* user, const int errorCode, const uint16_t address) {
  auto* self = static_cast<GameBoyActivity*>(user);
  if (!self) return;

  LOG_ERR(TAG, "Walnut-CGB fatal error=%d addr=0x%04X", errorCode, address);

  if (!self->saveCartRam()) {
    LOG_ERR(TAG, "Emergency CGB save failed");
  }
}

void GameBoyActivity::writeGameBoyLine(const uint8_t* pixels, const uint_fast8_t line) {
  if (!pixels || line >= GAMEBOY_HEIGHT || !frameBuffers[writeFrame]) {
    return;
  }

  std::memcpy(frameBuffers[writeFrame] + static_cast<size_t>(line) * GAMEBOY_WIDTH, pixels, GAMEBOY_WIDTH);
}

void GameBoyActivity::fatalError(gb_s* state, enum gb_error_e errorCode, const uint16_t addr) {
  auto* self = state ? static_cast<GameBoyActivity*>(state->direct.priv) : nullptr;

  if (!self) {
    ESP.restart();
    while (true) delay(1000);
  }

  LOG_ERR(TAG, "Peanut-GB fatal error=%d addr=0x%04X", static_cast<int>(errorCode), addr);

  if (!self->saveCartRam()) {
    LOG_ERR(TAG, "Emergency save failed before restart: %s", self->savePath.c_str());
  }

  ESP.restart();
  while (true) delay(1000);
}

const char* GameBoyActivity::initErrorString(const int errorCode) {
  switch (static_cast<gb_init_error_e>(errorCode)) {
    case GB_INIT_CARTRIDGE_UNSUPPORTED:
      return "Unsupported cartridge";

    case GB_INIT_INVALID_CHECKSUM:
      return "Invalid ROM checksum";

    case GB_INIT_NO_ERROR:
      return "";

    default:
      return "Unknown Game Boy ROM error";
  }
}

#endif  // CROSSINK_GAMEBOY