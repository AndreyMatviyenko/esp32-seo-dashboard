#include "settings_store.h"
#include <Preferences.h>
#include <new>

namespace dashboard {
namespace settings_store {
namespace {
// The active record is confirmed; pending is a complete candidate. Separate
// blobs preserve the previous configuration across an interrupted setup.
bool readSettings(const char* key, dashboard::DeviceConfig& value) {
  Preferences store;
  if (!store.begin("seo-settings", true)) return false;
  const size_t length = store.getBytesLength(key);
  bool ok = false;
  DeviceConfig candidate;
  if (length == kRecordSize) {
    // V1 stored only network/display settings. Preserve those, with Google
    // connections disabled until configured through the browser.
    uint8_t bytes[kRecordSize];
    ok = store.getBytes(key, bytes, sizeof(bytes)) == sizeof(bytes) && decode(bytes, sizeof(bytes), candidate.network);
  } else if (length > 0 && length <= kMaxConfigBytes) {
    auto* bytes = new (std::nothrow) char[length + 1];
    if (bytes) {
      if (store.getBytes(key, bytes, length) == length) {
        bytes[length] = 0;
        ok = decodeConfig(String(bytes), candidate);
      }
      delete[] bytes;
    }
  }
  store.end();
  // Invalid or incomplete records leave the caller's defaults unchanged.
  if (ok) value = candidate;
  return ok;
}
bool writeSettings(const char* key, const dashboard::DeviceConfig& value) {
  if (validateNetworks(value) || validateTitle(value.title) || validateConnections(value)) return false;
  String bytes = encodeConfig(value);
  // This serialization contains credentials: persist it, never log or send it.
  if (!bytes.length() || bytes.length() > kMaxConfigBytes) return false;
  // An allocation failure inside cJSON may produce incomplete JSON rather than
  // an empty result. Never replace a complete record with a partial encoding.
  DeviceConfig verified;
  if (!decodeConfig(bytes, verified)) return false;
  Preferences store;
  if (!store.begin("seo-settings", false)) return false;
  const bool ok = store.putBytes(key, bytes.c_str(), bytes.length()) == bytes.length();
  store.end();
  return ok;
}
bool removePending() {
  Preferences store;
  if (!store.begin("seo-settings", false)) return false;
  const bool ok = !store.isKey("pending") || store.remove("pending");
  store.end();
  return ok;
}
}

bool begin() {
  Preferences store;
  if (!store.begin("seo-settings", false)) return false;
  // Migrate only the obsolete random setup password. Device settings and
  // provider caches remain intact; the public factory password lives in code.
  if (store.isKey("admin")) store.remove("admin");
  store.end();
  return true;
}
bool loadActive(DeviceConfig& value) { return readSettings("active", value); }
bool loadPending(DeviceConfig& value) { return readSettings("pending", value); }
bool saveActive(const DeviceConfig& value) { return writeSettings("active", value); }
bool stage(const DeviceConfig& value) { return writeSettings("pending", value); }
bool discardPending() { return removePending(); }
}
}
