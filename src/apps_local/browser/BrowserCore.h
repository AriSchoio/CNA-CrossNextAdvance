#pragma once

#include <string>
#include <vector>

namespace browser {

struct Link {
  std::string label;
  std::string url;
};

struct Page {
  std::string title;
  std::string text;
  std::vector<Link> links;
};

bool normalizeUrl(const std::string& input, std::string& output);
bool resolveLink(const std::string& baseUrl, const std::string& href, std::string& output);
Page parsePage(const std::string& html, const std::string& baseUrl, size_t maxLinks = 64);

}  // namespace browser