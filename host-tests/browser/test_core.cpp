#include <cstdio>
#include <cstdlib>
#include <string>

#include "apps_local/browser/BrowserCore.h"

namespace {
int checks = 0;
int failures = 0;
#define CHECK(value)                                           \
  do {                                                         \
    ++checks;                                                  \
    if (!(value)) {                                            \
      ++failures;                                              \
      std::printf("FAIL %d: %s\n", __LINE__, #value);         \
    }                                                          \
  } while (0)
}  // namespace

int main() {
  std::string url;
  CHECK(browser::normalizeUrl("example.org/a", url) && url == "https://example.org/a");
  CHECK(browser::normalizeUrl(" HTTP://example.org ", url) && url == "http://example.org/");
  CHECK(!browser::normalizeUrl("javascript:alert(1)", url));
  CHECK(!browser::normalizeUrl("https://user:pass@example.org/", url));
  CHECK(!browser::normalizeUrl("https://bad host.example/", url));

  CHECK(browser::resolveLink("https://example.org/a/page", "../next", url) && url == "https://example.org/next");
  CHECK(browser::resolveLink("https://example.org/a/page", "/next", url) && url == "https://example.org/next");
  CHECK(browser::resolveLink("https://example.org/a/page", "//cdn.example/file", url) &&
        url == "https://cdn.example/file");
  CHECK(!browser::resolveLink("https://example.org/a", "mailto:test@example.org", url));
  CHECK(!browser::resolveLink("https://example.org/a", "#section", url));

  const auto page = browser::parsePage(
      "<html><head><title>Example &amp; test</title><script>hidden secret()</script></head><body>"
      "<h1>Welcome</h1><p>Hello <a href='/about'>about us</a>.</p>"
      "<a href='javascript:alert(1)'>bad</a><a href='https://other.example/x'>Other</a></body></html>",
      "https://example.org/home");
  CHECK(page.title == "Example & test");
  CHECK(page.text.find("Welcome") != std::string::npos);
  CHECK(page.text.find("hidden secret") == std::string::npos);
  CHECK(page.links.size() == 2);
  CHECK(page.links.size() > 0 && page.links[0].label == "about us" &&
        page.links[0].url == "https://example.org/about");
  CHECK(page.links.size() > 1 && page.links[1].url == "https://other.example/x");
  std::printf("Browser core: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}