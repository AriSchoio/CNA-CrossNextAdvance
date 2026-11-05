#include "InstalledAppsActivity.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "../../activities/goodies/GoodyModuleActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"

namespace {
constexpr char APP_ROOT[] = "/.crossnext/apps";
constexpr char GOODIES_ROOT[] = "/debug/goodies";

std::string titleFromFolder(const std::string& folder) {
  std::string title = folder.substr(folder.find_last_of('/') + 1);
  if (!title.empty()) title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
  return title;
}
}

InstalledAppsActivity::InstalledAppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("InstalledApps", renderer, mappedInput) {}

std::unique_ptr<Activity> InstalledAppsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<InstalledAppsActivity>(renderer, mappedInput);
}

void InstalledAppsActivity::onEnter() {
  Activity::onEnter();
  scanPackages();
  requestUpdate();
}

void InstalledAppsActivity::scanPackages() {
  packages.clear();

  auto addPackage = [this](const std::string& directory, const std::string& wasmPath) {
    if (!Storage.exists(wasmPath.c_str())) return;
    std::string title = titleFromFolder(directory);
    std::string version;
    const std::string manifestPath = directory + "/manifest.json";
    if (Storage.exists(manifestPath.c_str())) {
      std::string raw;
      if (Storage.readFileToString("APP", manifestPath.c_str(), 8 * 1024, raw)) {
        JsonDocument doc;
        if (!deserializeJson(doc, raw) && doc["name"].is<const char*>()) title = doc["name"].as<const char*>();
        if (doc["version"].is<const char*>()) version = doc["version"].as<const char*>();
      }
    }
    packages.push_back({std::move(title), std::move(version), wasmPath});
  };

  HalFile root = Storage.open(APP_ROOT);
  if (root && root.isDirectory()) {
    for (HalFile entry = root.openNextFile(); entry; entry = root.openNextFile()) {
      if (!entry.isDirectory()) continue;
      char folder[128];
      if (!entry.getName(folder, sizeof(folder))) continue;
      if (folder[0] == '.') continue;
      const std::string directory = std::string(APP_ROOT) + "/" + folder;
      addPackage(directory, directory + "/app.wasm");
    }
  }

  HalFile legacyRoot = Storage.open(GOODIES_ROOT);
  if (legacyRoot && legacyRoot.isDirectory()) {
    for (HalFile entry = legacyRoot.openNextFile(); entry; entry = legacyRoot.openNextFile()) {
      if (entry.isDirectory()) continue;
      char filename[128];
      if (!entry.getName(filename, sizeof(filename))) continue;
      std::string name(filename);
      std::string lower = name;
      std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
      if (lower.size() < 5 || lower.substr(lower.size() - 5) != ".wasm") continue;
      std::string path = std::string(GOODIES_ROOT) + "/" + name;
      std::string title = name.substr(0, name.size() - 5);
      packages.push_back({std::move(title), "legacy", std::move(path)});
    }
  }

  std::sort(packages.begin(), packages.end(), [](const Package& a, const Package& b) { return a.title < b.title; });
  if (selected >= static_cast<int>(packages.size())) selected = packages.empty() ? 0 : static_cast<int>(packages.size()) - 1;
}

void InstalledAppsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (packages.empty()) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected = std::max(0, selected - 1);
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected = std::min(static_cast<int>(packages.size()) - 1, selected + 1);
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto& package = packages[selected];
    startActivityForResult(std::make_unique<GoodyModuleActivity>(renderer, mappedInput, package.title, package.path),
                           [this](const ActivityResult&) { scanPackages(); requestUpdate(); });
  }
}

void InstalledAppsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "INSTALLED APPS");

  if (packages.empty()) {
    UITheme::drawCenteredWrappedText(renderer,
                                     Rect{metrics.listSidePadding, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing,
                                          renderer.getScreenWidth() - metrics.listSidePadding * 2,
                                          renderer.getScreenHeight() / 2},
                                     UI_12_FONT_ID, "No WASM apps installed\nCopy app.wasm to /.crossnext/apps/<name>/", 6);
  } else {
    const int bodyY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int rowHeight = std::max(52, renderer.getLineHeight(UI_12_FONT_ID) * 2 + 12);
    for (size_t i = 0; i < packages.size(); ++i) {
      const int y = bodyY + static_cast<int>(i) * rowHeight;
      if (y + rowHeight >= renderer.getScreenHeight() - metrics.buttonHintsHeight) break;
      if (static_cast<int>(i) == selected) renderer.fillRect(0, y, renderer.getScreenWidth(), rowHeight);
      renderer.drawText(UI_12_FONT_ID, metrics.listSidePadding + 8, y + 8,
                        packages[i].title.c_str(), static_cast<int>(i) == selected, EpdFontFamily::BOLD);
      if (!packages[i].version.empty()) {
        renderer.drawText(SMALL_FONT_ID, metrics.listSidePadding + 8, y + 34, packages[i].version.c_str());
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), packages.empty() ? "" : tr(STR_OPEN), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
