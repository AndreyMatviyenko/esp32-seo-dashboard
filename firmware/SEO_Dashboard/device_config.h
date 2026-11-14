#pragma once

#include <Arduino.h>
#include "cJSON.h"
#include "settings_config.h"

namespace dashboard {
enum Service : uint8_t { AdSense, GA4, GSC, CrUX, ServiceCount };
extern const char* const serviceNames[ServiceCount];
constexpr size_t kMaxConfigBytes = 4096;
// Font12 is 7 px wide. Twenty characters leave an 8 px gap before the longest
// header (PARTIAL + HH:MM DD.MM) on the 296 px display.
constexpr size_t kMaxTitleLength = 20;
constexpr const char* kDefaultTitle = "SEO DASHBOARD";

struct Connection {
  bool enabled = false;
  // resource: GA4 property ID, exact GSC site identifier, or CrUX origin.
  // AdSense discovers its single account and does not use this field.
  String clientId, clientSecret, refreshToken, resource, apiKey;
  // Structural completeness only; API access is checked during polling.
  bool ready(Service service) const;
};
struct DeviceConfig {
  Settings network;
  // Network zero is network's inherited NetworkProfile; these are slots 2/3.
  NetworkProfile fallbackNetworks[kMaxWifiNetworks - 1];
  String title = kDefaultTitle;
  Connection services[ServiceCount];
};

const NetworkProfile& networkProfile(const DeviceConfig& config, size_t index);
NetworkProfile& networkProfile(DeviceConfig& config, size_t index);
size_t networkCount(const DeviceConfig& config);
bool sameNetworks(const DeviceConfig& first, const DeviceConfig& second);
const char* validateNetworks(const DeviceConfig& config);
const char* networksFromForm(cJSON* json, const DeviceConfig& current, DeviceConfig& candidate);

const char* validateTitle(const String& title);

// Public metadata contains presence flags, never passwords, tokens or API keys.
// The caller owns the returned JSON object and must call cJSON_Delete.
cJSON* publicConfig(const DeviceConfig& config);
// Full private serialization for NVS, never for HTTP responses or logging.
String encodeConfig(const DeviceConfig& config);
// Assigns config only after the entire versioned record passes validation.
bool decodeConfig(const String& json, DeviceConfig& config);
// Blank secret fields retain saved values; clearCredentials explicitly removes
// them. candidate may be partially changed on error and must then be discarded.
const char* connectionsFromForm(cJSON* json, const DeviceConfig& current, DeviceConfig& candidate);
const char* validateConnections(const DeviceConfig& config);
// Cache fingerprint only; no secret values are returned to the settings page.
String connectionIdentity(const Connection& connection);
}
