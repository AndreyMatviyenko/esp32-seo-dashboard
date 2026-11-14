#include "wifi_networks.h"
#include <WiFi.h>

namespace dashboard {
namespace {
constexpr uint32_t kJoinTimeoutMs = 20000;

bool beginAttempt(const DeviceConfig& config, size_t index) {
  // Await a real disconnect even when changing only the password. A previous
  // connection must never confirm a candidate with different credentials.
  if (!WiFi.disconnect(false, false, 1000)) return false;
  const auto& network = networkProfile(config, index);
  Serial.printf("WIFI: trying configured network %u/%u\n", unsigned(index + 1), unsigned(networkCount(config)));
  WiFi.begin(network.ssid, network.password);
  return true;
}
}

bool WifiNetworks::join(const DeviceConfig& config) {
  retrying_ = attempting_ = false;
  for (size_t i = 0; i < networkCount(config); ++i) {
    if (!beginAttempt(config, i)) continue;
    const uint32_t started = millis();
    while (!elapsed(millis(), started, kJoinTimeoutMs)) {
      if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == networkProfile(config, i).ssid) {
        Serial.printf("WIFI: configured network %u connected\n", unsigned(i + 1));
        return true;
      }
      delay(50);
    }
  }
  // Cancel the final candidate before rollback or AP setup. Otherwise a late
  // connection could be mistaken for success under the previous configuration.
  WiFi.disconnect(false, false, 1000);
  return false;
}

void WifiNetworks::startRetry() {
  index_ = 0;
  retrying_ = true;
  attempting_ = false;
}

bool WifiNetworks::tick(const DeviceConfig& config) {
  if (WiFi.status() == WL_CONNECTED) {
    if (attempting_ && WiFi.SSID() != networkProfile(config, index_).ssid) {
      WiFi.disconnect(false, false, 1000);
    } else {
      retrying_ = attempting_ = false;
      return false; // Keep a working network; do not roam when priority 1 returns.
    }
  }
  if (!retrying_) return false;
  if (attempting_) {
    if (!elapsed(millis(), startedAt_, kJoinTimeoutMs)) return false;
    WiFi.disconnect(false, false, 1000);
    attempting_ = false;
    ++index_;
  }
  if (index_ >= networkCount(config)) {
    retrying_ = false;
    Serial.println("WIFI: all configured networks unavailable");
    return true;
  }
  if (!beginAttempt(config, index_)) { ++index_; return false; }
  startedAt_ = millis();
  attempting_ = true;
  return false;
}
}
