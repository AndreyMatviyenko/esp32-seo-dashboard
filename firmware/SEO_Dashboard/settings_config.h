#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "display_options.h"

namespace dashboard {

constexpr size_t kMaxWifiNetworks = 3;

struct NetworkProfile {
  // SSID capacity is measured in UTF-8 bytes, with one extra byte for NUL.
  char ssid[33] = {};
  char password[65] = {};
};

// The first network keeps its legacy fields. Further profiles live in
// DeviceConfig; no SSID/password is duplicated when a list is reordered.
struct Settings : NetworkProfile {
  uint16_t intervalMinutes = 15;
  bool dark = true;
  bool rotate180 = false;
  char timeZone[40] = "Europe/Kyiv";
};

inline const char* validateNetwork(const NetworkProfile& s) {
  if (!memchr(s.ssid, 0, sizeof(s.ssid)) || !s.ssid[0]) return "Enter a Wi-Fi network name (1-32 bytes).";
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s.ssid); *p; ++p)
    if (*p < 32 || *p == 127) return "The network name contains a control character.";
  if (!memchr(s.password, 0, sizeof(s.password))) return "Wi-Fi password is too long.";
  const size_t length = strlen(s.password);
  if (length && length < 8) return "Wi-Fi password must have 8-63 characters, or a 64-digit hex key.";
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = s.password[i];
    if (c < 32 || c > 126) return "Wi-Fi password must contain ASCII characters.";
    if (length == 64 && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return "A 64-character Wi-Fi key must be hexadecimal.";
  }
  return nullptr;
}

inline const char* validate(const Settings& s) {
  if (const char* error = validateNetwork(s)) return error;
  if (s.intervalMinutes < 5 || s.intervalMinutes > 1440) return "Choose an interval from 5 to 1440 minutes.";
  if (!memchr(s.timeZone, 0, sizeof(s.timeZone)) || !findTimeZone(s.timeZone))
    return "Choose a supported time zone.";
  return nullptr;
}

// Legacy V1 network-only record, retained for migration. New settings writes
// use the V4 JSON record in device_config.cpp (which also reads V2/V3). Explicit
// bytes avoid struct padding and firmware ABI differences.
constexpr size_t kRecordSize = 110;
inline uint32_t checksum(const uint8_t* bytes, size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < length; ++i) value = (value ^ bytes[i]) * 16777619UL;
  return value;
}
inline void encode(const Settings& s, uint8_t (&bytes)[kRecordSize]) {
  memset(bytes, 0, sizeof(bytes));
  memcpy(bytes, "SEO1", 4);
  bytes[4] = 1;
  bytes[5] = s.dark ? 1 : 0;
  bytes[6] = s.intervalMinutes >> 8;
  bytes[7] = s.intervalMinutes & 255;
  memcpy(bytes + 8, s.ssid, sizeof(s.ssid));
  memcpy(bytes + 41, s.password, sizeof(s.password));
  const uint32_t hash = checksum(bytes, 106);
  for (int i = 0; i < 4; ++i) bytes[106+i] = (hash >> (8*i)) & 255;
}
inline bool decode(const uint8_t* bytes, size_t length, Settings& out) {
  if (length != kRecordSize || memcmp(bytes, "SEO1", 4) || bytes[4] != 1 || bytes[5] > 1) return false;
  uint32_t hash = 0;
  for (int i = 0; i < 4; ++i) hash |= uint32_t(bytes[106+i]) << (8*i);
  if (hash != checksum(bytes, 106)) return false;
  Settings candidate;
  candidate.dark = bytes[5] == 1;
  candidate.intervalMinutes = (uint16_t(bytes[6]) << 8) | bytes[7];
  memcpy(candidate.ssid, bytes + 8, sizeof(candidate.ssid));
  memcpy(candidate.password, bytes + 41, sizeof(candidate.password));
  if (validate(candidate)) return false;
  out = candidate;
  return true;
}
inline bool elapsed(uint32_t now, uint32_t start, uint32_t interval) {
  // Unsigned subtraction keeps the short polling/restart timers valid across
  // the millis() rollover, unlike comparing absolute deadline values.
  return uint32_t(now - start) >= interval;
}
inline bool restartDue(bool pending, uint32_t now, uint32_t savedAt, bool busy) {
  return pending && !busy && elapsed(now, savedAt, 2000);
}

inline const char* networkFromForm(const NetworkProfile& current, const char* ssid, const char* password,
                                   bool openNetwork, NetworkProfile& out) {
  if (!ssid || !password || strlen(ssid) > 32 || strlen(password) > 64)
    return "Wi-Fi name or password is too long.";
  NetworkProfile candidate = current;
  memset(candidate.ssid, 0, sizeof(candidate.ssid));
  memcpy(candidate.ssid, ssid, strlen(ssid));
  // A blank password retains the current one only for the same secured SSID.
  // Choosing an open network is an explicit operation, not an empty input.
  if (openNetwork) memset(candidate.password, 0, sizeof(candidate.password));
  else if (*password) {
    memset(candidate.password, 0, sizeof(candidate.password));
    memcpy(candidate.password, password, strlen(password));
  } else if (strcmp(candidate.ssid, current.ssid) || !current.password[0])
    return "Enter a password for the new network, or select Open network.";
  if (const char* error = validateNetwork(candidate)) return error;
  out = candidate;
  return nullptr;
}

inline const char* fromForm(const Settings& current, const char* ssid, const char* password,
                            bool openNetwork, bool dark, uint16_t interval, Settings& out) {
  Settings candidate = current;
  if (const char* error = networkFromForm(current, ssid, password, openNetwork, candidate)) return error;
  candidate.dark = dark;
  candidate.intervalMinutes = interval;
  if (const char* error = validate(candidate)) return error;
  out = candidate;
  return nullptr;
}

template<class Join, class Save>
bool confirmCandidate(const Settings& candidate, Settings& active, bool& confirmed, Join join, Save save) {
  // Preserve the old record unless both association and persistence succeed.
  if (!join(candidate) || !save(candidate)) return false;
  active = candidate;
  confirmed = true;
  return true;
}
} // namespace dashboard
