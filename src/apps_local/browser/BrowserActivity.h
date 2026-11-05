#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "../ui/ToyboxWrappedText.h"
#include "BrowserCore.h"

class BrowserActivity final : public Activity {
 public:
  BrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Browser", renderer, mappedInput) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Home, Loading, Reading, Links, Notice };
  enum class HistoryChange : uint8_t { Push, Back, Forward, Reload };

  void openAddress();
  void requestPage(const std::string&, HistoryChange);
  void loadPage();
  void finishPage(browser::Page&&);
  void turnPage(int);
  void back();
  void route(int, int);
  void showNotice(const char*, View returnTo);

  View view_ = View::Home;
  View noticeReturn_ = View::Home;
  HistoryChange historyChange_ = HistoryChange::Push;
  browser::Page page_;
  std::string currentUrl_;
  std::string pendingUrl_;
  std::string notice_;
  std::vector<std::string> history_;
  int historyIndex_ = -1;
  int linkSelection_ = 0;
  int linkTop_ = 0;
  int linkPageSize_ = 7;
  uint32_t topLine_ = 0;
  uint32_t lineCount_ = 0;
  uint16_t visibleLines_ = 0;
  bool pendingFetch_ = false;
  bool interactionsReady_ = false;
  toybox::WrappedText wrapped_;
  toybox::Interactions interactions_;
};