#include "BrowserActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "../../activities/RenderLock.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "components/SideButtonNavigation.h"

namespace {
namespace fui = freeink::ui;
enum : fui::ActionId {
  ActionAddress = 900,
  ActionHome,
  ActionPrevious,
  ActionNext,
  ActionLinks,
  ActionOpenLink,
  ActionRetry,
  ActionReturnPage
};
constexpr size_t kMaxResponseBytes = 64 * 1024;
constexpr size_t kMaxPageTextBytes = 32 * 1024;
constexpr size_t kMaxHistory = 12;
constexpr int kLinksPerPage = 7;

void drawChrome(toybox::Screen& screen, const char* title, const char* subtitle = nullptr) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.rightLabel = subtitle;
  header.back = screen.device().hasTouch;
  pixelui::header(screen, header);
  backui::publish(header.back, pixelui::geometry(screen.device()).back);
}

void prose(toybox::Screen& screen, fui::Rect rect, const char* value, int lines = 4) {
  fittext::draw(screen.target(), rect, value, toybox::kLibraryFontId, lines, fui::TextAlign::Left);
}

void button(toybox::Screen& screen, fui::Rect rect, const char* label, fui::ActionId action, int value = 0,
            bool primary = false) {
  dashboard::button(screen, rect, label, action, value, primary);
}
}  // namespace

std::unique_ptr<Activity> BrowserActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  auto app = makeUniqueNoThrow<BrowserActivity>(renderer, mappedInput);
  if (!app) LOG_ERR("BROWSER", "OOM: BrowserActivity");
  return app;
}

void BrowserActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  requestUpdate();
}

void BrowserActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void BrowserActivity::openAddress() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BROWSER_ADDRESS), currentUrl_,
                                                            512, InputType::Url);
  if (!keyboard) {
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_B6FD93), view_);
    return;
  }
  interactionsReady_ = false;
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    requestPage(std::get<KeyboardResult>(result.data).text, HistoryChange::Push);
  });
}

void BrowserActivity::requestPage(const std::string& value, const HistoryChange change) {
  std::string normalized;
  if (!browser::normalizeUrl(value, normalized)) {
    showNotice(tr(STR_BROWSER_INVALID_ADDRESS), view_);
    return;
  }
  pendingUrl_ = std::move(normalized);
  historyChange_ = change;
  noticeReturn_ = view_ == View::Notice ? noticeReturn_ : view_;
  if (WiFi.status() == WL_CONNECTED) {
    pendingFetch_ = true;
    view_ = View::Loading;
    requestUpdate();
    return;
  }
  WiFi.mode(WIFI_STA);
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!picker) {
    pendingUrl_.clear();
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_B6FD93), noticeReturn_);
    return;
  }
  interactionsReady_ = false;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
      pendingUrl_.clear();
      showNotice(tr(STR_BROWSER_WIFI_REQUIRED), noticeReturn_);
      return;
    }
    pendingFetch_ = true;
    view_ = View::Loading;
    requestUpdate();
  });
}

void BrowserActivity::showNotice(const char* message, const View returnTo) {
  notice_ = message ? message : tr(STR_BROWSER_LOAD_FAILED);
  noticeReturn_ = returnTo;
  view_ = View::Notice;
  interactionsReady_ = false;
  requestUpdate();
}

void BrowserActivity::loadPage() {
  const std::string requestedUrl = pendingUrl_;
  pendingFetch_ = false;
  std::string response;
  bool tooLarge = false;
  const bool fetched = HttpDownloader::fetchUrl(
      requestedUrl, [&response, &tooLarge](const uint8_t* bytes, const size_t count) {
        if (count > kMaxResponseBytes - response.size()) {
          tooLarge = true;
          return false;
        }
        response.append(reinterpret_cast<const char*>(bytes), count);
        return true;
      });
  if (!fetched) {
    char message[160];
    if (tooLarge)
      std::snprintf(message, sizeof(message), "%s", tr(STR_BROWSER_PAGE_TOO_LARGE));
    else
      std::snprintf(message, sizeof(message), "%s (%d)", tr(STR_BROWSER_LOAD_FAILED), HttpDownloader::lastStatus());
    showNotice(message, noticeReturn_);
    return;
  }
  auto parsed = browser::parsePage(response, requestedUrl);
  if (parsed.text.size() > kMaxPageTextBytes) {
    parsed.text.resize(kMaxPageTextBytes);
    while (!parsed.text.empty() && (static_cast<uint8_t>(parsed.text.back()) & 0xC0) == 0x80) parsed.text.pop_back();
    parsed.text += "\n\n[Page text shortened.]";
  }
  if (parsed.text.empty()) {
    showNotice(tr(STR_BROWSER_NO_READABLE_TEXT), noticeReturn_);
    return;
  }
  finishPage(std::move(parsed));
}

void BrowserActivity::finishPage(browser::Page&& page) {
  currentUrl_ = pendingUrl_;
  pendingUrl_.clear();
  if (page.title.empty()) page.title = currentUrl_;
  page_ = std::move(page);
  switch (historyChange_) {
    case HistoryChange::Push:
      if (historyIndex_ + 1 < static_cast<int>(history_.size())) history_.resize(historyIndex_ + 1);
      history_.push_back(currentUrl_);
      if (history_.size() > kMaxHistory) history_.erase(history_.begin());
      historyIndex_ = static_cast<int>(history_.size()) - 1;
      break;
    case HistoryChange::Back:
      historyIndex_ = std::max(0, historyIndex_ - 1);
      break;
    case HistoryChange::Forward:
      historyIndex_ = std::min(static_cast<int>(history_.size()) - 1, historyIndex_ + 1);
      break;
    case HistoryChange::Reload:
      break;
  }
  topLine_ = 0;
  lineCount_ = 0;
  linkTop_ = linkSelection_ = 0;
  view_ = View::Reading;
  interactionsReady_ = false;
  requestUpdate();
}

void BrowserActivity::turnPage(const int delta) {
  if (view_ != View::Reading || !visibleLines_) return;
  if (delta > 0) {
    if (topLine_ + visibleLines_ < lineCount_) topLine_ += visibleLines_;
  } else {
    topLine_ = topLine_ > visibleLines_ ? topLine_ - visibleLines_ : 0;
  }
  requestUpdate();
}

void BrowserActivity::back() {
  if (view_ == View::Links) {
    view_ = View::Reading;
    requestUpdate();
  } else if (view_ == View::Notice) {
    view_ = noticeReturn_;
    requestUpdate();
  } else if (view_ == View::Reading && historyIndex_ > 0) {
    requestPage(history_[historyIndex_ - 1], HistoryChange::Back);
  } else if (view_ == View::Reading) {
    view_ = View::Home;
    requestUpdate();
  } else {
    shelf::leave(renderer, mappedInput);
  }
}

void BrowserActivity::route(const int action, const int value) {
  switch (action) {
    case ActionAddress:
      openAddress();
      return;
    case ActionHome:
      view_ = View::Home;
      requestUpdate();
      return;
    case ActionPrevious:
      if (historyIndex_ > 0) requestPage(history_[historyIndex_ - 1], HistoryChange::Back);
      return;
    case ActionNext:
      if (historyIndex_ >= 0 && historyIndex_ + 1 < static_cast<int>(history_.size()))
        requestPage(history_[historyIndex_ + 1], HistoryChange::Forward);
      return;
    case ActionLinks:
      if (!page_.links.empty()) {
        view_ = View::Links;
        linkSelection_ = linkTop_ = 0;
        requestUpdate();
      }
      return;
    case ActionReturnPage:
      view_ = View::Reading;
      requestUpdate();
      return;
    case ActionOpenLink:
      if (view_ == View::Links && value >= 0 && value < static_cast<int>(page_.links.size()))
        requestPage(page_.links[static_cast<size_t>(value)].url, HistoryChange::Push);
      else if (view_ == View::Home && value >= 0 && value < static_cast<int>(history_.size()))
        requestPage(history_[static_cast<size_t>(value)], HistoryChange::Reload);
      return;
    case ActionRetry:
      if (!pendingUrl_.empty()) requestPage(pendingUrl_, historyChange_);
      else if (!currentUrl_.empty()) requestPage(currentUrl_, HistoryChange::Reload);
      else openAddress();
      return;
    default:
      return;
  }
}

void BrowserActivity::loop() {
  if (pendingFetch_ && surfaceRevealed()) {
    loadPage();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (view_ == View::Reading) {
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down) || swipe == MappedInputManager::SwipeDir::Up) {
      turnPage(1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
        mappedInput.wasReleased(MappedInputManager::Button::Up) || swipe == MappedInputManager::SwipeDir::Down) {
      turnPage(-1);
      return;
    }
  } else if (view_ == View::Links && (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                                     mappedInput.wasReleased(MappedInputManager::Button::Down))) {
    const int delta = mappedInput.wasReleased(MappedInputManager::Button::Down) ? 1 : -1;
    linkSelection_ = std::clamp(linkSelection_ + delta, 0, std::max(0, static_cast<int>(page_.links.size()) - 1));
    if (linkSelection_ < linkTop_) linkTop_ = linkSelection_;
    if (linkSelection_ >= linkTop_ + linkPageSize_) linkTop_ = linkSelection_ - linkPageSize_ + 1;
    requestUpdate();
    return;
  }
  if (view_ == View::Links && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    route(ActionOpenLink, linkSelection_);
    return;
  }
  int x = 0, y = 0;
  if (!interactionsReady_ || !surfaceRevealed() || !sidebuttons::poll(interactions_, mappedInput, x, y)) return;
  fui::InputSnapshot input{};
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(x);
  input.touchY = static_cast<int16_t>(y);
  const auto event = sidebuttons::route(interactions_, input, mappedInput, *this);
  route(event.action, event.value);
}

void BrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions_, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  const auto device = screen.device();
  const auto body = pixelui::geometry(device).body;
  const auto footer = fui::makeRect(body.x, body.bottom() - 52, body.width, 48);

  if (view_ == View::Home) {
    drawChrome(screen, tr(STR_UI_WEB_BROWSER));
    dashboard::prose(screen, fui::makeRect(body.x + 12, body.y + 8, body.width - 24, 76), tr(STR_BROWSER_INTRO),
                     fui::TextAlign::Center, 4);
    button(screen, fui::makeRect(body.x + 24, body.y + 96, body.width - 48, 72), tr(STR_BROWSER_OPEN_ADDRESS),
           ActionAddress, 0, true);
    int y = body.y + (device.width > device.height ? 176 : 206);
    dashboard::text(screen, fui::makeRect(body.x + 8, y, body.width - 16, 32), tr(STR_BROWSER_RECENT), 3,
                    fui::TextAlign::Left);
    y += 38;
    const int rows = device.width > device.height ? 2 : 4;
    const int rowHeight = device.width > device.height ? 44 : 52;
    const int first = std::max(0, static_cast<int>(history_.size()) - rows);
    for (int n = static_cast<int>(history_.size()) - 1; n >= first; --n) {
      const auto row = fui::makeRect(body.x + 8, y, body.width - 16, rowHeight);
      button(screen, row, history_[static_cast<size_t>(n)].c_str(), ActionOpenLink, n);
      y += rowHeight + 6;
    }
  } else if (view_ == View::Loading) {
    drawChrome(screen, tr(STR_BROWSER_LOADING));
    dashboard::prose(screen, fui::makeRect(body.x + 16, body.y + 80, body.width - 32, body.height - 100),
                     pendingUrl_.c_str(), fui::TextAlign::Center, 5);
  } else if (view_ == View::Notice) {
    drawChrome(screen, tr(STR_UI_WEB_BROWSER));
    dashboard::prose(screen, fui::makeRect(body.x + 16, body.y + 48, body.width - 32, body.height - 120),
                     notice_.c_str(), fui::TextAlign::Center, 6);
    button(screen, footer, tr(STR_BROWSER_RETRY), ActionRetry, 0, true);
  } else if (view_ == View::Links) {
    drawChrome(screen, tr(STR_BROWSER_LINKS), currentUrl_.c_str());
    linkPageSize_ = std::clamp((body.height - 72) / 52, 1, kLinksPerPage);
    const int first = linkTop_;
    const int count = std::min(linkPageSize_, static_cast<int>(page_.links.size()) - first);
    const int rowHeight = std::min(64, (body.height - 72) / std::max(1, count));
    for (int n = 0; n < count; ++n) {
      const int index = first + n;
      const auto row = fui::makeRect(body.x + 4, body.y + n * rowHeight, body.width - 8, rowHeight - 4);
      button(screen, row, page_.links[static_cast<size_t>(index)].label.c_str(), ActionOpenLink, index,
             index == linkSelection_);
    }
    button(screen, footer, tr(STR_BROWSER_RETURN_PAGE), ActionReturnPage);
  } else {
    drawChrome(screen, page_.title.c_str());
    const auto address = fui::makeRect(body.x + 4, body.y, body.width - 8, 44);
    button(screen, address, currentUrl_.c_str(), ActionAddress);
    const auto reading = fui::makeRect(body.x + 8, address.bottom() + 8, body.width - 16,
                                       body.bottom() - footer.height - address.bottom() - 20);
    auto style = screen.theme().bodyText;
    style.align = fui::TextAlign::Left;
    style.maxLines = 0;
    const int16_t lineHeight = target.lineHeight(style.font);
    visibleLines_ = fui::textAreaVisibleLines(reading, lineHeight);
    lineCount_ = wrapped_.lineCount(target, reading.width, page_.text.c_str(), style);
    wrapped_.draw(target, reading, page_.text.c_str(), style, topLine_);
    const int half = (footer.width - 16) / 3;
    button(screen, fui::makeRect(footer.x, footer.y, half, footer.height), tr(STR_BROWSER_BACK), ActionPrevious, 0,
           historyIndex_ > 0);
    button(screen, fui::makeRect(footer.x + half + 8, footer.y, half, footer.height), tr(STR_BROWSER_LINKS),
           ActionLinks, 0, !page_.links.empty());
    button(screen, fui::makeRect(footer.x + 2 * (half + 8), footer.y, half, footer.height), tr(STR_BROWSER_FORWARD),
           ActionNext, 0, historyIndex_ >= 0 && historyIndex_ + 1 < static_cast<int>(history_.size()));
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Browser", &renderer);
  renderer.displayBuffer();
}