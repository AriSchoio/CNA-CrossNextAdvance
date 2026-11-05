#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../activities/Activity.h"

class InstalledAppsActivity final : public Activity {
 public:
  InstalledAppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Package {
    std::string title;
    std::string version;
    std::string path;
  };

  void scanPackages();

  std::vector<Package> packages;
  int selected = 0;
};
