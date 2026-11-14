#pragma once

#include <stddef.h>
#include <string.h>

namespace dashboard {
struct TimeZoneOption {
  const char* id;
  const char* label;
  const char* rule;
};

// Current/future clock rules, not a historical timezone database. IANA IDs are
// stored in NVS; ESP32's libc requires POSIX rules for localtime_r(). Keep this
// small table in sync with civil-time changes before a firmware release.
// Sources: https://github.com/eggert/tz and Espressif's System Time guide.
constexpr TimeZoneOption kTimeZones[] = {
  {"Europe/Kyiv", "Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"UTC", "UTC", "UTC0"},
  {"Europe/Warsaw", "Warsaw", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
  {"Europe/Berlin", "Berlin", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
  {"Europe/London", "London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
  {"America/New_York", "New York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
  {"America/Chicago", "Chicago", "CST6CDT,M3.2.0/2,M11.1.0/2"},
  {"America/Los_Angeles", "Los Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
  {"Asia/Dubai", "Dubai", "GST-4"},
  {"Asia/Kolkata", "Kolkata", "IST-5:30"},
  {"Asia/Tokyo", "Tokyo", "JST-9"},
};
constexpr size_t kTimeZoneCount = sizeof(kTimeZones) / sizeof(kTimeZones[0]);
constexpr const char* kDefaultTimeZone = "Europe/Kyiv";

inline const TimeZoneOption* findTimeZone(const char* id) {
  if (id) for (const auto& zone : kTimeZones)
    if (!strcmp(zone.id, id)) return &zone;
  return nullptr;
}
}
