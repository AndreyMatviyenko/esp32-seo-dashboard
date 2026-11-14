#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "dashboard_data.h"
#include "dashboard_display.h"
#include "dashboard_button.h"
#include "google_reports.h"
#include "metrics_cache.h"
#include "settings_portal.h"

namespace {
using namespace dashboard;
SettingsPortal portal;
MetricsCache cache;
DashboardData data;
DashboardButton button;
bool portalReady = false;
bool firstUpdatePending = true;
bool setupFrameShown = false;
uint32_t lastAttempt = 0;
bool networkWasConnected = false;

Status prepareNetwork() {
  // The portal owns network selection. Requests additionally need a valid UTC
  // clock for TLS certificates and last-success timestamps.
  if (WiFi.status() != WL_CONNECTED) return Status::WifiError;
  configTime(0, 0, "time.google.com", "pool.ntp.org");
  const uint32_t started = millis();
  while (time(nullptr) < 1700000000 && !elapsed(millis(), started, 15000)) delay(100);
  return time(nullptr) >= 1700000000 ? Status::Live : Status::ClockError;
}

String utcStamp(time_t value) {
  if (!value) return "never";
  struct tm date; gmtime_r(&value, &date);
  char buffer[24]; strftime(buffer, sizeof(buffer), "%m-%d %H:%M UTC", &date);
  return buffer;
}

void publishStatuses() {
  for (int i = 0; i < ServiceCount; ++i)
    portal.publishService(Service(i), statusLabel(data.providers[i].status), data.providers[i].updated);
  portal.publishDisplay(data.display);
}

void beginDisplayUpdate(DisplayFrame frame) {
  data.display.state = DisplayState::Updating;
  data.display.frame = frame;
  data.display.error[0] = 0;
  portal.publishDisplay(data.display);
}

void finishDisplayUpdate(const display::RefreshResult& result) {
  data.display.state = result.confirmed ? DisplayState::Ready : DisplayState::Error;
  strlcpy(data.display.error, result.error, sizeof(data.display.error));
  const time_t now = time(nullptr);
  if (result.confirmed && now >= 1700000000) data.display.lastSuccess = now;
  portal.publishDisplay(data.display);
}

Status fetchProvider(Service service, const Connection& connection) {
  switch (service) {
    case AdSense: return fetchAdSense(connection, data.adsense);
    case GA4: return fetchGa4(connection, data.ga4);
    case GSC: return fetchGsc(connection, data.gsc);
    case CrUX: return fetchCrux(connection, data.crux);
    default: return Status::Disabled;
  }
}

bool syncNetworkStatus() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    // A reconnect needs fresh reports before a provider can become LIVE again.
    if (!networkWasConnected) firstUpdatePending = true;
    networkWasConnected = true;
    return false;
  }
  networkWasConnected = false;
  bool changed = false;
  int enabled = 0;
  for (int i = 0; i < ServiceCount; ++i) {
    if (!portal.config().services[i].ready(Service(i))) continue;
    ++enabled;
    if (data.providers[i].status != Status::WifiError) {
      data.providers[i].status = Status::WifiError;
      changed = true;
    }
  }
  data.overall = enabled ? OverallStatus::Error : OverallStatus::Setup;
  if (changed) publishStatuses(); // Keep values and last-success times intact.
  return changed;
}

void redrawPage() {
  syncNetworkStatus();
  beginDisplayUpdate(DisplayFrame::Dashboard);
  finishDisplayUpdate(display::showPage(portal.config(), data, button.page(), WiFi.localIP().toString()));
}

void updateDashboard() {
  const DeviceConfig& config = portal.config();
  int enabled = 0, successful = 0;
  for (int i = 0; i < ServiceCount; ++i)
    if (config.services[i].ready(Service(i))) ++enabled;
  const Status network = enabled ? prepareNetwork() : Status::Disabled;

  // Providers update independently. Failures retain their last-known metrics;
  // request functions publish a complete report or leave their output intact.
  for (int i = 0; i < ServiceCount; ++i) {
    const Service service = Service(i);
    ProviderState& state = data.providers[i];
    const Connection& connection = config.services[i];
    if (!connection.ready(service)) { state.status = Status::Disabled; continue; }
    if (network != Status::Live) { state.status = network; continue; }

    // Reuse daily field data, but retry a failed request on the next cycle.
    // A cache hit never advances its last-success timestamp or writes to NVS.
    const time_t now = time(nullptr);
    const bool cachedCrux = service == CrUX && data.crux.detailsLoaded && state.updated > 0 && now >= state.updated &&
      now - state.updated < 86400 && (state.status == Status::Live || state.status == Status::Waiting);
    state.status = cachedCrux ? Status::Live : fetchProvider(service, connection);
    if (state.status == Status::Live) {
      ++successful;
      if (!cachedCrux) {
        state.updated = time(nullptr);
        if (!cache.save(service, connection, data))
          Serial.printf("CACHE: %s write failed; current values remain available in RAM\n", serviceNames[i]);
      }
    }
  }
  data.overall = !enabled ? OverallStatus::Setup : successful == enabled ? OverallStatus::Live :
    successful ? OverallStatus::Partial : OverallStatus::Error;
  syncNetworkStatus(); // Wi-Fi may have dropped during the blocking requests.
  publishStatuses();
  const char* labels[] = {"AdSense", "GA4", "GSC", "CWV"};
  for (int i = 0; i < ServiceCount; ++i) {
    if (!config.services[i].ready(Service(i))) continue;
    Serial.printf("%s: %s / last OK %s\n", labels[i], statusLabel(data.providers[i].status),
      utcStamp(data.providers[i].updated).c_str());
  }
  if (config.services[CrUX].ready(CrUX)) Serial.println("CWV data to " + data.crux.lastDate);
  Serial.println(statusLabel(data.overall));
  // Consume presses recorded during TLS before choosing the frame. Switching
  // pages never refetches providers or resets their last-success timestamps.
  button.update(config);
  redrawPage();
  // Start the interval after the complete cycle, including the display refresh.
  lastAttempt = millis();
}

void logResetReason() {
  const esp_reset_reason_t reason = esp_reset_reason();
  const char* label = "OTHER";
  switch (reason) {
    case ESP_RST_POWERON: label = "POWER ON"; break;
    case ESP_RST_EXT: label = "EXTERNAL RESET"; break;
    case ESP_RST_SW: label = "SOFTWARE RESET"; break;
    case ESP_RST_PANIC: label = "PANIC"; break;
    case ESP_RST_INT_WDT: label = "INTERRUPT WATCHDOG"; break;
    case ESP_RST_TASK_WDT: label = "TASK WATCHDOG"; break;
    case ESP_RST_WDT: label = "WATCHDOG"; break;
    case ESP_RST_BROWNOUT: label = "BROWNOUT"; break;
    case ESP_RST_USB: label = "USB RESET"; break;
    default: break;
  }
  Serial.printf("Reset reason: %s (%d)\n", label, int(reason));
}

void handleSerialCommand() {
  static String command;
  while (Serial.available()) {
    const char character = Serial.read();
    if (character == '\n' || character == '\r') {
      if (command == "setup") portal.openSetup();
      command = "";
    } else if (command.length() < 16) command += character;
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("SEO DASHBOARD / MULTI WIFI + ROTATION + TIME ZONE v1");
  button.begin();
  logResetReason();
  Serial.printf("Initial heap=%u; minimum=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  // Establish safe control-pin levels before potentially lengthy networking.
  const bool prepared = dashboard::display::prepare();
  Serial.printf("DISPLAY SPI PREPARE: %s\n", prepared ? "OK" : "FAILED");
  if (!prepared) {
    data.display.state = DisplayState::Error;
    strlcpy(data.display.error, "SPI_INIT_FAILED", sizeof(data.display.error));
  }
  portalReady = portal.begin();
  if (!portalReady) {
    Serial.println("SETTINGS ERROR: storage, network, or HTTP task initialization failed");
    return;
  }
  if (cache.begin()) cache.load(portal.config(), data);
  else Serial.println("CACHE: unavailable; continuing without persistence");
  publishStatuses();
  Serial.printf("Polling interval: %u minutes; USB command: setup\n", portal.settings().intervalMinutes);
}

void loop() {
  if (!portalReady) { delay(100); return; }
  // Only this task owns polling, Wi-Fi transitions, and display operations.
  // The HTTP worker stays available during blocking TLS and e-paper updates.
  handleSerialCommand();
  portal.tick();
  const bool networkStatusChanged = syncNetworkStatus();
  if (portal.takeSetupNotice()) {
    portal.setDashboardBusy(true);
    beginDisplayUpdate(DisplayFrame::Setup);
    finishDisplayUpdate(dashboard::display::showSetup(portal.config(), SettingsPortal::kSetupSsid, portal.setupPassword()));
    portal.setDashboardBusy(false);
    setupFrameShown = true;
  }
  // Setup instructions/password have priority. Do not replay setup-time clicks
  // later as dashboard navigation, including a button held as setup closes.
  if (portal.setupActive()) { button.discard(); delay(20); return; }
  if (setupFrameShown) { firstUpdatePending = true; setupFrameShown = false; }
  const bool pageChanged = button.update(portal.config());
  if (!portal.restartPending() && WiFi.status() == WL_CONNECTED &&
      (firstUpdatePending || dashboard::elapsed(millis(), lastAttempt, portal.intervalMs()))) {
    portal.setDashboardBusy(true);
    updateDashboard();
    portal.setDashboardBusy(false);
    firstUpdatePending = false;
  } else if (!portal.restartPending() && (pageChanged || networkStatusChanged)) {
    portal.setDashboardBusy(true);
    redrawPage();
    portal.setDashboardBusy(false);
  }
  delay(20);
}
