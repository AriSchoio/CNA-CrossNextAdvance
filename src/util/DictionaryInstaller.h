#pragma once

#include <string>

namespace DictionaryInstaller {

struct ImportResult {
  bool success = false;
  std::string dictionaryName;
  std::string error;
};

ImportResult importZip(const std::string& zipPath, const std::string& requestedName = "");

}  // namespace DictionaryInstaller
