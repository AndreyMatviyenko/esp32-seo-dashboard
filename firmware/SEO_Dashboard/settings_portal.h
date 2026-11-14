#pragma once

#include <Arduino.h>
#include "settings_http.h"
#include <DNSServer.h>
#include <atomic>
#include "dashboard_data.h"
#include "wifi_networks.h"

class SettingsPortal {
public:
  // begin() and tick() run in the Arduino task. After begin(), the worker owns
  // HTTP/DNS handling; it stages changes and never draws or reconfigures Wi-Fi.
  bool begin();
  void tick(); // Arduino task only; Wi-Fi transitions never interrupt a Google cycle.
  void openSetup();
  bool takeSetupNotice() { return setupNotice_.exchange(false); }
  bool setupActive() const { return apActive_.load(); }
  bool restartPending() const { return restart_.load(); }
  void setDashboardBusy(bool value) { busy_.store(value); }
  const dashboard::Settings& settings() const { return config_.network; }
  const dashboard::DeviceConfig& config() const { return config_; }
  void publishService(dashboard::Service service, const String& state, int64_t lastSuccess);
  void publishDisplay(const dashboard::DisplayStatus& status);
  const char* setupPassword() const { return kSetupPassword; }
  uint32_t intervalMs() const { return uint32_t(config_.network.intervalMinutes) * 60000UL; }
  static constexpr const char* kSetupSsid = "seo-dashboard-setup";
  // Public factory default requested for this project, not a private credential.
  static constexpr const char* kSetupPassword = "seodashboard";
private:
  SettingsWebServer server_{80};
  DNSServer dns_;
  dashboard::WifiNetworks wifi_;
  // Immutable after begin(): both tasks may read it without sharing mutable
  // Strings. A successful Save takes effect through pending NVS + reboot.
  dashboard::DeviceConfig config_;
  // Fixed-size snapshots cross the task boundary under statusMux_; allocations
  // and response serialization happen outside the critical section.
  struct ServiceStatus { char state[32] = "WAITING"; int64_t lastSuccess = 0; };
  ServiceStatus serviceStatus_[dashboard::ServiceCount];
  dashboard::DisplayStatus displayStatus_;
  portMUX_TYPE statusMux_ = portMUX_INITIALIZER_UNLOCKED;
  String csrf_, bootMessage_;
  std::atomic<bool> busy_{false}, restart_{false}, apActive_{false}, setupNotice_{false};
  std::atomic<uint32_t> savedAt_{0};
  uint32_t retryAt_ = 0, connectedAt_ = 0, setupAt_ = 0;
  bool manualSetup_ = false, wasConnected_ = false, confirmed_ = false;
  TaskHandle_t task_ = nullptr;
  bool startAp();
  bool allowedHost();
  bool authenticate();
  void headers();
  void sendError(int status, const char* message);
  void sendSettings();
  bool authorizeSettingsSave();
  void saveSettings(const String& body);
  static void run(void* self);
};
