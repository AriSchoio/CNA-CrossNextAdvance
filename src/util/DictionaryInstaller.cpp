#include "DictionaryInstaller.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ZipFile.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

std::string baseName(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string safeFolderName(std::string name) {
  if (name.size() > 31) name.resize(31);
  for (char& c : name) {
    unsigned char ch = static_cast<unsigned char>(c);
    if (ch < 32 || ch == '/' || ch == '\\' || c == '.') {
      c = '_';
    } else if (!std::isalnum(ch) && c != ' ' && c != '-' && c != '_' && ch >= 128) {
      // Preserve UTF-8 bytes, already validated by callers for printable text.
    } else if (!std::isalnum(ch) && c != ' ' && c != '-' && c != '_') {
      c = '_';
    }
  }
  while (!name.empty() && name.back() == '_') name.pop_back();
  if (name.empty()) name = "Imported Dictionary";
  return name;
}

bool allowedDictionaryFile(const std::string& name) {
  const std::string lower = [&]() {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
  }();
  return lower.size() >= 4 && (
    lower.rfind(".ifo") == lower.size() - 4 ||
    lower.rfind(".idx") == lower.size() - 4 ||
    lower.rfind(".dict") == lower.size() - 5 ||
    lower.rfind(".dict.dz") == lower.size() - 8 ||
    lower.rfind(".syn") == lower.size() - 4 ||
    lower.rfind(".css") == lower.size() - 4);
}

std::string stemFromIndex(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
  if (lower.size() > 4 && lower.ends_with(".idx")) return name.substr(0, name.size() - 4);
  return "";
}

std::string defaultName(const std::string& zipPath) {
  std::string name = baseName(zipPath);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos) name.resize(dot);
  return safeFolderName(name);
}

}  // namespace

namespace DictionaryInstaller {

ImportResult importZip(const std::string& zipPath, const std::string& requestedName) {
  ImportResult result;
  ZipFile zip(zipPath);
  if (!zip.open()) {
    result.error = "The uploaded file is not a readable ZIP archive";
    return result;
  }

  std::vector<std::string> entries;
  if (!zip.enumerateFilePaths([&entries](std::string_view entry) { entries.emplace_back(entry); })) {
    zip.close();
    result.error = "Could not read the ZIP central directory";
    return result;
  }
  zip.close();

  std::string stem;
  std::vector<std::string> files;
  for (const std::string& entry : entries) {
    if (entry.empty() || entry.back() == '/') continue;
    const std::string name = baseName(entry);
    if (!allowedDictionaryFile(name)) continue;
    files.push_back(name);
    if (stem.empty()) {
      stem = stemFromIndex(name);
    } else if (!stemFromIndex(name).empty() && stemFromIndex(name) != stem) {
      result.error = "The ZIP contains multiple dictionary index stems";
      return result;
    }
  }

  if (stem.empty()) {
    result.error = "No StarDict .idx index was found";
    return result;
  }

  bool hasData = false;
  std::string stemLower = stem;
  std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), [](unsigned char c) { return std::tolower(c); });
  for (const std::string& entry : entries) {
    const std::string name = baseName(entry);
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    if (lower == stemLower + ".dict" || lower == stemLower + ".dict.dz") {
      hasData = true;
      break;
    }
  }
  if (!hasData) {
    result.error = "The StarDict index has no matching .dict or .dict.dz data file";
    return result;
  }

  result.dictionaryName = safeFolderName(requestedName.empty() ? defaultName(zipPath) : requestedName);
  const std::string folder = "/dictionaries/" + result.dictionaryName;
  if (Storage.exists(folder.c_str())) {
    result.error = "A dictionary folder named '" + result.dictionaryName + "' already exists";
    return result;
  }
  if (!Storage.mkdir("/dictionaries", true) || !Storage.mkdir(folder.c_str(), true)) {
    result.error = "Could not create the dictionary folder on the SD card";
    return result;
  }

  bool ok = true;
  for (const std::string& entryPath : entries) {
    if (entryPath.empty() || entryPath.back() == '/') continue;
    const std::string name = baseName(entryPath);
    if (!allowedDictionaryFile(name)) continue;
    const std::string target = folder + "/" + name;
    HalFile out;
    if (!Storage.openFileForWrite("DICT", target.c_str(), out)) {
      ok = false;
      result.error = "Could not write " + name + " to the SD card";
      break;
    }
    ZipFile source(zipPath);
    if (!source.readFileToStream(entryPath.c_str(), out, 4096)) {
      out.close();
      Storage.remove(target.c_str());
      ok = false;
      result.error = "Failed to extract " + name;
      break;
    }
    out.close();
  }

  if (!ok) return result;
  result.success = true;
  LOG_DBG("DICT", "Installed dictionary '%s' from %s", result.dictionaryName.c_str(), zipPath.c_str());
  return result;
}

}  // namespace DictionaryInstaller
