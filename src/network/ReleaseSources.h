#pragma once

namespace release_sources {
constexpr const char* const kUrls[] = {
    "https://api.github.com/repos/AriSchoio/CNA-CrossNextAdvance/releases/latest",
};
constexpr int kCount = static_cast<int>(sizeof(kUrls) / sizeof(kUrls[0]));
}  // namespace release_sources
