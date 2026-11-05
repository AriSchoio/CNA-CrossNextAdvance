#include "QuotesStore.h"

#include <HalStorage.h>
#include <Logging.h>
namespace quotes {
namespace {
constexpr char path[] = "/.crosspoint/quotes.bin", pending[] = "/.crosspoint/quotes.tmp",
               backup[] = "/.crosspoint/quotes.bak";
constexpr uint8_t kVersion = 2;
uint32_t fold(uint32_t h, const uint8_t* p, size_t n) {
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
void put32(uint8_t* b, uint32_t v) {
  for (int n = 0; n < 4; ++n) b[n] = uint8_t(v >> (n * 8));
}
uint32_t get32(const uint8_t* b) {
  uint32_t v = 0;
  for (int n = 0; n < 4; ++n) v |= uint32_t(b[n]) << (n * 8);
  return v;
}
size_t noteLength(const Entry& e) {
  size_t n = 0;
  while (n <= kNoteMax && e.note[n]) ++n;
  return n;
}
void record(uint8_t* b, const Entry& e, size_t noteBytes) {
  for (int n = 0; n < 8; ++n) b[n] = uint8_t(e.key >> (n * 8));
  b[8] = e.flags;
  b[9] = static_cast<uint8_t>(noteBytes);
  std::memcpy(b + 10, e.note, noteBytes);
}
bool read(const char* name, Entry* entries, int count, bool apply) {
  HalFile f;
  if (!Storage.openFileForRead("QUOTES", name, f)) return false;
  uint8_t b[10 + kNoteMax]{};
  if (f.read(b, 8) != 8 || std::memcmp(b, "QTS1", 4) || (b[4] != 1 && b[4] != kVersion) || b[5]) return false;
  const uint8_t version = b[4];
  const int size = int(b[6]) | (int(b[7]) << 8);
  if (size > kMax) return false;
  uint32_t crc = fold(2166136261u, b, 8);
  uint32_t expectedSize = 12;
  for (int i = 0; i < size; ++i) {
    const size_t headerBytes = version == 1 ? 9 : 10;
    if (f.read(b, headerBytes) != int(headerBytes) || (b[8] & ~(kFavorite | kHidden))) return false;
    const size_t noteBytes = version == 1 ? 0 : b[9];
    if (noteBytes > kNoteMax || (noteBytes && f.read(b + 10, noteBytes) != int(noteBytes))) return false;
    const size_t recordBytes = headerBytes + noteBytes;
    crc = fold(crc, b, recordBytes);
    expectedSize += recordBytes;
  }
  if (f.size() != expectedSize) return false;
  if (f.read(b, 4) != 4 || get32(b) != crc) return false;
  if (!apply) return true;
  if (!f.seekSet(8)) return false;
  for (int i = 0; i < size; ++i) {
    const size_t headerBytes = version == 1 ? 9 : 10;
    if (f.read(b, headerBytes) != int(headerBytes)) return false;
    const size_t noteBytes = version == 1 ? 0 : b[9];
    if (noteBytes > kNoteMax || (noteBytes && f.read(b + 10, noteBytes) != int(noteBytes))) return false;
    uint64_t key = 0;
    for (int n = 0; n < 8; ++n) key |= uint64_t(b[n]) << (8 * n);
    for (int n = 0; n < count; ++n) {
      if (entries[n].key != key) continue;
      entries[n].flags = b[8];
      entries[n].note[0] = 0;
      if (noteBytes) {
        std::memcpy(entries[n].note, b + 10, noteBytes);
        entries[n].note[noteBytes] = 0;
      }
    }
  }
  return true;
}
}  // namespace
Load loadFlags(Entry* entries, int count) {
  if (count < 0 || count > kMax || (!entries && count)) return Load::Error;
  const bool main = Storage.exists(path), bak = Storage.exists(backup);
  if (main) {
    HalFile f;
    uint8_t b[5]{};
    if (Storage.openFileForRead("QUOTES", path, f) && f.read(b, 5) == 5 && !std::memcmp(b, "QTS1", 4) &&
      b[4] > kVersion) {
      LOG_ERR("QUOTES", "Future quote state");
      return Load::Error;
    }
  }
  if (!main && !bak) return Load::Missing;
  for (int n = 0; n < count; ++n) {
    entries[n].flags = 0;
    entries[n].note[0] = 0;
  }
  if (main && read(path, entries, count, true)) return Load::Ok;
  for (int n = 0; n < count; ++n) {
    entries[n].flags = 0;
    entries[n].note[0] = 0;
  }
  if (bak && read(backup, entries, count, true)) return Load::Ok;
  for (int n = 0; n < count; ++n) {
    entries[n].flags = 0;
    entries[n].note[0] = 0;
  }
  LOG_ERR("QUOTES", "Unreadable quote state");
  return Load::Error;
}
static bool writeFlags(const Entry* entries, int count) {
  if (count < 0 || count > kMax || (!entries && count)) return false;
  for (const char* name : {path, backup}) {
    HalFile f;
    uint8_t b[5]{};
    if (Storage.exists(name) && Storage.openFileForRead("QUOTES", name, f) && f.read(b, 5) == 5 &&
        !std::memcmp(b, "QTS1", 4) && b[4] > kVersion)
      return false;
  }
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  int kept = 0;
  for (int n = 0; n < count; ++n) {
    if (noteLength(entries[n]) > kNoteMax) return false;
    if (entries[n].flags || entries[n].note[0]) ++kept;
  }
  {
    HalFile f;
    if (!Storage.openFileForWrite("QUOTES", pending, f)) return false;
    uint8_t b[10 + kNoteMax] = {'Q', 'T', 'S', '1', kVersion, 0, uint8_t(kept), uint8_t(kept >> 8)};
    uint32_t crc = fold(2166136261u, b, 8);
    if (f.write(b, 8) != 8) return false;
    for (int n = 0; n < count; ++n) {
      const size_t noteBytes = noteLength(entries[n]);
      if (!entries[n].flags && !noteBytes) continue;
      record(b, entries[n], noteBytes);
      const size_t recordBytes = 10 + noteBytes;
      crc = fold(crc, b, recordBytes);
      if (f.write(b, recordBytes) != recordBytes) return false;
    }
    put32(b, crc);
    if (f.write(b, 4) != 4) return false;
    f.flush();
  }
  if (!read(pending, nullptr, 0, false)) return false;
  // Never replace a validated backup with an unreadable primary.
  if (Storage.exists(path)) {
    if (read(path, nullptr, 0, false)) {
      if (Storage.exists(backup) && !Storage.remove(backup)) return false;
      if (!Storage.rename(path, backup)) return false;
    } else if (!Storage.remove(path))
      return false;
  }
  if (!Storage.rename(pending, path)) {
    LOG_ERR("QUOTES", "Could not publish quote state");
    return false;
  }
  return true;
}
bool saveFlags(const Entry* entries, int count) {
  const bool ok = writeFlags(entries, count);
  if (!ok) LOG_ERR("QUOTES", "Could not save quote state");
  return ok;
}
}  // namespace quotes
