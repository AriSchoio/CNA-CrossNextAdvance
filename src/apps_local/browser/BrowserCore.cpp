#include "BrowserCore.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "util/HtmlToPlainText.h"

namespace browser {
namespace {

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  if (first >= last) return {};
  return std::string(first, last);
}

bool startsWithInsensitive(const std::string_view value, const std::string_view prefix) {
  if (value.size() < prefix.size()) return false;
  for (size_t n = 0; n < prefix.size(); ++n)
    if (std::tolower(static_cast<unsigned char>(value[n])) != std::tolower(static_cast<unsigned char>(prefix[n])))
      return false;
  return true;
}

size_t findInsensitive(const std::string& value, const std::string_view needle, const size_t from = 0) {
  if (needle.empty() || from > value.size()) return std::string::npos;
  const std::string_view view(value);
  for (size_t at = from; at + needle.size() <= value.size(); ++at)
    if (startsWithInsensitive(view.substr(at), needle)) return at;
  return std::string::npos;
}

size_t tagEnd(const std::string& html, size_t start) {
  char quote = 0;
  for (size_t at = start; at < html.size(); ++at) {
    const char c = html[at];
    if (quote) {
      if (c == quote) quote = 0;
    } else if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '>') {
      return at;
    }
  }
  return std::string::npos;
}

std::string attribute(const std::string& tag, const std::string_view wanted) {
  size_t at = 0;
  while (at < tag.size() && !std::isspace(static_cast<unsigned char>(tag[at])) && tag[at] != '>') ++at;
  while (at < tag.size()) {
    while (at < tag.size() && (std::isspace(static_cast<unsigned char>(tag[at])) || tag[at] == '/')) ++at;
    const size_t nameStart = at;
    while (at < tag.size() && (std::isalnum(static_cast<unsigned char>(tag[at])) || tag[at] == '-' || tag[at] == ':' ||
                               tag[at] == '_'))
      ++at;
    if (nameStart == at) break;
    const std::string name = tag.substr(nameStart, at - nameStart);
    while (at < tag.size() && std::isspace(static_cast<unsigned char>(tag[at]))) ++at;
    if (at >= tag.size() || tag[at] != '=') continue;
    ++at;
    while (at < tag.size() && std::isspace(static_cast<unsigned char>(tag[at]))) ++at;
    if (at >= tag.size()) break;
    char quote = 0;
    if (tag[at] == '\'' || tag[at] == '"') quote = tag[at++];
    const size_t valueStart = at;
    if (quote) {
      while (at < tag.size() && tag[at] != quote) ++at;
    } else {
      while (at < tag.size() && !std::isspace(static_cast<unsigned char>(tag[at])) && tag[at] != '>') ++at;
    }
    if (startsWithInsensitive(name, wanted) && name.size() == wanted.size())
      return htmlToPlainText(tag.substr(valueStart, at - valueStart));
    if (quote && at < tag.size()) ++at;
  }
  return {};
}

std::string origin(const std::string& url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) return {};
  const size_t end = url.find_first_of("/?#", scheme + 3);
  return url.substr(0, end == std::string::npos ? url.size() : end);
}

std::string pathPart(const std::string& url) {
  const size_t start = url.find("://");
  if (start == std::string::npos) return "/";
  const size_t pathStart = url.find('/', start + 3);
  if (pathStart == std::string::npos) return "/";
  const size_t end = url.find_first_of("?#", pathStart);
  return url.substr(pathStart, end == std::string::npos ? url.size() - pathStart : end - pathStart);
}

std::string normalizePath(std::string path) {
  std::string query;
  const size_t queryAt = path.find('?');
  if (queryAt != std::string::npos) {
    query = path.substr(queryAt);
    path.resize(queryAt);
  }
  std::vector<std::string> parts;
  size_t at = 0;
  while (at < path.size()) {
    while (at < path.size() && path[at] == '/') ++at;
    const size_t end = path.find('/', at);
    const std::string part = path.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (part == "..") {
      if (!parts.empty()) parts.pop_back();
    } else if (!part.empty() && part != ".") {
      parts.push_back(part);
    }
    if (end == std::string::npos) break;
    at = end + 1;
  }
  std::string result = "/";
  for (size_t n = 0; n < parts.size(); ++n) {
    if (n) result += '/';
    result += parts[n];
  }
  if (path.size() > 1 && path.back() == '/' && result.back() != '/') result += '/';
  return result + query;
}

std::string stripInvisible(const std::string& html) {
  std::string visible;
  visible.reserve(html.size());
  for (size_t at = 0; at < html.size();) {
    if (html.compare(at, 4, "<!--") == 0) {
      const size_t end = html.find("-->", at + 4);
      at = end == std::string::npos ? html.size() : end + 3;
      continue;
    }
    if (html[at] == '<' && at + 1 < html.size() && std::isalpha(static_cast<unsigned char>(html[at + 1]))) {
      const size_t end = tagEnd(html, at + 1);
      if (end != std::string::npos) {
        std::string tag = html.substr(at + 1, end - at - 1);
        size_t nameEnd = 0;
        while (nameEnd < tag.size() && std::isalnum(static_cast<unsigned char>(tag[nameEnd]))) ++nameEnd;
        const std::string name = tag.substr(0, nameEnd);
        const bool hidden = startsWithInsensitive(name, "script") || startsWithInsensitive(name, "style") ||
                            startsWithInsensitive(name, "noscript");
        if (hidden && name.size() == 6 && startsWithInsensitive(name, "script")) {
          const size_t close = findInsensitive(html, "</script", end + 1);
          const size_t closeEnd = close == std::string::npos ? std::string::npos : tagEnd(html, close + 2);
          at = closeEnd == std::string::npos ? html.size() : closeEnd + 1;
          continue;
        }
        if (hidden && name.size() == 5 && startsWithInsensitive(name, "style")) {
          const size_t close = findInsensitive(html, "</style", end + 1);
          const size_t closeEnd = close == std::string::npos ? std::string::npos : tagEnd(html, close + 2);
          at = closeEnd == std::string::npos ? html.size() : closeEnd + 1;
          continue;
        }
      }
    }
    visible.push_back(html[at++]);
  }
  return visible;
}

}  // namespace

bool normalizeUrl(const std::string& input, std::string& output) {
  output.clear();
  std::string value = trim(input);
  if (value.empty() || value.size() > 512) return false;
  if (value.find("://") == std::string::npos) value = "https://" + value;
  const size_t schemeEnd = value.find("://");
  std::string scheme = value.substr(0, schemeEnd);
  std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  if (scheme != "http" && scheme != "https") return false;
  const size_t hostStart = schemeEnd + 3;
  const size_t hostEnd = value.find_first_of("/?#", hostStart);
  const std::string authority = value.substr(hostStart, hostEnd == std::string::npos ? value.size() - hostStart
                                                                                   : hostEnd - hostStart);
  if (authority.empty() || authority.find('@') != std::string::npos ||
      authority.find_first_of(" \t\r\n\\") != std::string::npos)
    return false;
  for (const unsigned char c : authority)
    if (!(std::isalnum(c) || c == '.' || c == '-' || c == ':' || c == '[' || c == ']' || c == '%')) return false;
  std::string remainder = hostEnd == std::string::npos ? "/" : value.substr(hostEnd);
  if (remainder.empty() || remainder[0] == '?') remainder = "/" + remainder;
  if (remainder[0] != '/') return false;
  std::string normalized = scheme + "://" + authority + normalizePath(remainder);
  const size_t fragment = normalized.find('#');
  if (fragment != std::string::npos) normalized.resize(fragment);
  output = std::move(normalized);
  return true;
}

bool resolveLink(const std::string& baseUrl, const std::string& href, std::string& output) {
  output.clear();
  const std::string target = trim(href);
  if (target.empty() || target[0] == '#') return false;
  if (startsWithInsensitive(target, "javascript:") || startsWithInsensitive(target, "data:") ||
      startsWithInsensitive(target, "mailto:") || startsWithInsensitive(target, "tel:"))
    return false;
  std::string base;
  if (!normalizeUrl(baseUrl, base)) return false;
  std::string candidate;
  if (target.rfind("//", 0) == 0) {
    const size_t scheme = base.find("://");
    candidate = base.substr(0, scheme) + ":" + target;
  } else if (target.find("://") != std::string::npos) {
    candidate = target;
  } else if (target[0] == '/') {
    candidate = origin(base) + target;
  } else if (target[0] == '?') {
    const size_t query = base.find('?');
    candidate = (query == std::string::npos ? base : base.substr(0, query)) + target;
  } else {
    const std::string path = pathPart(base);
    const size_t query = path.find('?');
    const std::string cleanPath = path.substr(0, query);
    const size_t slash = cleanPath.rfind('/');
    candidate = origin(base) + cleanPath.substr(0, slash == std::string::npos ? 0 : slash + 1) + target;
  }
  return normalizeUrl(candidate, output);
}

Page parsePage(const std::string& html, const std::string& baseUrl, const size_t maxLinks) {
  Page page;
  const size_t titleStart = findInsensitive(html, "<title");
  if (titleStart != std::string::npos) {
    const size_t start = tagEnd(html, titleStart + 1);
    const size_t end = start == std::string::npos ? std::string::npos : findInsensitive(html, "</title", start + 1);
    if (start != std::string::npos && end != std::string::npos)
      page.title = trim(htmlToPlainText(html.substr(start + 1, end - start - 1)));
  }
  const std::string visible = stripInvisible(html);
  page.text = trim(htmlToPlainText(visible));

  for (size_t at = 0; page.links.size() < maxLinks;) {
    const size_t anchor = findInsensitive(visible, "<a", at);
    if (anchor == std::string::npos) break;
    const size_t end = tagEnd(visible, anchor + 1);
    if (end == std::string::npos) break;
    const std::string tag = visible.substr(anchor + 1, end - anchor - 1);
    if (tag.size() < 2 || (!std::isspace(static_cast<unsigned char>(tag[1])) && tag[1] != '>')) {
      at = end + 1;
      continue;
    }
    const std::string href = attribute(tag, "href");
    const size_t close = findInsensitive(visible, "</a", end + 1);
    if (close == std::string::npos) break;
    const size_t closeEnd = tagEnd(visible, close + 2);
    at = closeEnd == std::string::npos ? close + 3 : closeEnd + 1;
    std::string url;
    if (!resolveLink(baseUrl, href, url)) continue;
    std::string label = trim(htmlToPlainText(visible.substr(end + 1, close - end - 1)));
    if (label.empty()) label = url;
    if (label.size() > 160) label.resize(160);
    if (std::none_of(page.links.begin(), page.links.end(), [&url](const Link& link) { return link.url == url; }))
      page.links.push_back({std::move(label), std::move(url)});
  }
  return page;
}

}  // namespace browser