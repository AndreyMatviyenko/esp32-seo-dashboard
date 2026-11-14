#include "settings_portal.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_random.h>
#include <math.h>
#include "cJSON.h"
#include "settings_assets.h"
#include "settings_store.h"

namespace {
String randomHex(size_t length) {
  String result;
  result.reserve(length);
  const char hex[] = "0123456789abcdef";
  while (result.length() < length) {
    uint8_t value;
    esp_fill_random(&value, 1);
    result += hex[value & 15];
  }
  return result;
}

}

bool SettingsPortal::begin() {
  WiFi.persistent(false); // Only our confirmed NVS record controls network selection.
  WiFi.setHostname("seo-dashboard");
  WiFi.mode(WIFI_STA); // Enable RF entropy before generating the CSRF token.
  WiFi.setAutoReconnect(false); // Our priority list owns every reconnect attempt.
  if (!dashboard::settings_store::begin()) return false;
  csrf_ = randomHex(32);
  // A new boot invalidates POSTs from old browser tabs. The UI must reload its
  // public configuration before submitting with this boot's CSRF token.
  // With no valid NVS record, defaults contain no networks or Google access.
  // Only browser setup can configure a fresh device.
  dashboard::DeviceConfig active, pending;
  bool confirmed = dashboard::settings_store::loadActive(active);
  Serial.printf("SETTINGS LOAD: %s\n", confirmed ? "confirmed configuration loaded" : "no valid confirmed configuration");
  config_ = active;
  if (dashboard::settings_store::loadPending(pending)) {
    const bool networkChanged = !confirmed || !dashboard::sameNetworks(active, pending);
    Serial.println(networkChanged ? "SETTINGS APPLY: checking changed Wi-Fi settings" :
      "SETTINGS APPLY: Wi-Fi unchanged; applying settings");
    // Only new network credentials need confirmation. A temporary Wi-Fi outage
    // must not discard unrelated changes to theme, interval, or Google access.
    const bool joined = !networkChanged || wifi_.join(pending);
    const bool saved = joined && dashboard::settings_store::saveActive(pending);
    if (saved) {
      config_ = pending;
      confirmed = true;
      bootMessage_ = networkChanged ? "Settings applied. Wi-Fi connection verified." : "Settings applied.";
      Serial.println("SETTINGS APPLY: configuration saved as active");
    } else {
      WiFi.disconnect(false, false);
      bootMessage_ = "New settings could not be confirmed. Previous settings were kept.";
      Serial.println(joined ? "SETTINGS APPLY: NVS write failed; previous configuration kept" :
        "SETTINGS APPLY: Wi-Fi confirmation failed; previous configuration kept");
    }
    // A failed candidate is consumed too; subsequent boots use the old active
    // record instead of repeatedly trying an unreachable network.
    if (!dashboard::settings_store::discardPending()) return false;
  }
  confirmed_=confirmed;
  if (confirmed && WiFi.status()!=WL_CONNECTED) wifi_.join(config_);
  wasConnected_ = WiFi.status() == WL_CONNECTED;
  if (wasConnected_) {
    connectedAt_ = millis();
    if (MDNS.begin("seo-dashboard")) MDNS.addService("http", "tcp", 80);
    Serial.println("SETTINGS: http://" + WiFi.localIP().toString() + "/ (admin; default setup password)");
  } else if (!startAp()) return false;
  retryAt_ = millis();

  const char* collected[] = {"Origin", "X-CSRF-Token", "Content-Type"};
  server_.collectHeaders(collected, 3);
  server_.on("/", HTTP_GET, [this]() {
    if (!authenticate()) return;
    headers(); server_.send_P(200, "text/html; charset=utf-8", SETTINGS_HTML);
  });
  server_.on("/api/settings", HTTP_GET, [this]() { if (authenticate()) sendSettings(); });
  if (!server_.onSettingsSave([this]() { return authorizeSettingsSave(); },
      [this](const String& body) { saveSettings(body); },
      [this](int status, const char* error) { sendError(status, error); })) return false;
  server_.onNotFound([this]() {
    if (apActive_.load() && server_.method() == HTTP_GET && server_.uri() != "/api/settings") {
      headers(); server_.sendHeader("Location", "http://192.168.4.1/"); server_.send(302, "text/plain", "Open setup");
    } else if (authenticate()) sendError(404, "Not found.");
  });
  server_.begin();
  // Below the Arduino task: HTTP cannot preempt its timing-sensitive RST pulse.
  if (xTaskCreate(run, "settings-http", 8192, this, 0, &task_) != pdPASS) { server_.stop(); return false; }
  return true;
}

bool SettingsPortal::startAp() {
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0)) ||
      !WiFi.softAP(kSetupSsid, kSetupPassword)) return false;
  apActive_.store(true);
  setupNotice_.store(true);
  setupAt_ = millis();
  Serial.println("SETUP: seo-dashboard-setup / http://192.168.4.1/ / password on display");
  return true;
}
void SettingsPortal::openSetup() {
  manualSetup_ = true;
  if (apActive_.load()) { setupAt_ = millis(); setupNotice_.store(true); }
  else startAp();
}
void SettingsPortal::tick() {
  // Restart only between complete polling/display cycles, after allowing the
  // HTTP success response time to reach the browser.
  if (dashboard::restartDue(restart_.load(), millis(), savedAt_.load(), busy_.load())) ESP.restart();
  if (WiFi.status() == WL_CONNECTED) wifi_.tick(config_);
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected_) {
    connectedAt_ = millis();
    if (MDNS.begin("seo-dashboard")) MDNS.addService("http", "tcp", 80);
    Serial.println("SETTINGS: http://" + WiFi.localIP().toString() + "/");
  }
  if (!connected && confirmed_) {
    // A lost connection starts at priority 1. After an exhausted cycle, wait
    // one minute; offline setup remains available while retries are pending.
    if (!wifi_.retrying() && (wasConnected_ || dashboard::elapsed(millis(), retryAt_, 60000)))
      wifi_.startRetry();
    if (wifi_.tick(config_)) {
      retryAt_ = millis();
      if (!apActive_.load()) startAp();
    }
  }

  if (apActive_.load() && connected && WiFi.softAPgetStationNum() == 0 &&
      dashboard::elapsed(millis(), manualSetup_ ? setupAt_ : connectedAt_, manualSetup_ ? 300000 : 30000)) {
    apActive_.store(false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    manualSetup_ = false;
  }
  wasConnected_ = connected;
}
void SettingsPortal::run(void* context) {
  // WebServer and DNSServer each have one owner. Cross-task commands use atomic
  // flags; Wi-Fi transitions and all SPI operations remain in the Arduino task.
  auto* portal = static_cast<SettingsPortal*>(context);
  bool dnsRunning = false;
  for (;;) {
    const bool ap = portal->apActive_.load();
    if (ap && !dnsRunning) dnsRunning = portal->dns_.start(53, "*", IPAddress(192,168,4,1));
    if (!ap && dnsRunning) { portal->dns_.stop(); dnsRunning = false; }
    if (dnsRunning) portal->dns_.processNextRequest();
    portal->server_.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void SettingsPortal::headers() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("X-Content-Type-Options", "nosniff");
  server_.sendHeader("Referrer-Policy", "no-referrer");
  server_.sendHeader("Content-Security-Policy", SETTINGS_CSP);
}
bool SettingsPortal::allowedHost() {
  String host = server_.hostHeader();
  host.toLowerCase();
  if (host.endsWith(":80")) host.remove(host.length() - 3);
  return host == "seo-dashboard.local" || host == "seo-dashboard" ||
    (apActive_.load() && host == "192.168.4.1") ||
    (WiFi.status() == WL_CONNECTED && host == WiFi.localIP().toString());
}
bool SettingsPortal::authenticate() {
  headers();
  if (!allowedHost()) { sendError(421, "Open the device IP address or seo-dashboard.local."); return false; }
  if (server_.authenticate("admin", kSetupPassword)) return true;
  if (server_.uri()=="/api/settings" && server_.method()==HTTP_POST)
    Serial.println("SETTINGS SAVE: authentication required; nothing written");
  server_.requestSetupAuthentication(server_.uri()=="/api/settings");
  return false;
}
void SettingsPortal::sendError(int status, const char* message) {
  cJSON* response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "error", message);
  char* json = cJSON_PrintUnformatted(response);
  headers(); server_.send(status, "application/json", json ? json : "{}");
  cJSON_free(json); cJSON_Delete(response);
}
void SettingsPortal::sendSettings() {
  cJSON* response=dashboard::publicConfig(config_);
  ServiceStatus snapshot[dashboard::ServiceCount];
  dashboard::DisplayStatus display;
  portENTER_CRITICAL(&statusMux_);
  memcpy(snapshot,serviceStatus_,sizeof(snapshot));
  display=displayStatus_;
  portEXIT_CRITICAL(&statusMux_);
  cJSON* displayJson=cJSON_AddObjectToObject(response,"display");
  cJSON_AddStringToObject(displayJson,"state",dashboard::statusLabel(display.state));
  cJSON_AddStringToObject(displayJson,"frame",dashboard::frameLabel(display.frame));
  cJSON_AddStringToObject(displayJson,"error",display.error);
  cJSON_AddNumberToObject(displayJson,"lastSuccess",double(display.lastSuccess));
  cJSON* services=cJSON_GetObjectItemCaseSensitive(response,"services");
  for (int i=0;i<dashboard::ServiceCount;++i) {
    cJSON* item=cJSON_GetObjectItemCaseSensitive(services,dashboard::serviceNames[i]);
    cJSON_AddStringToObject(item,"state",config_.services[i].ready(dashboard::Service(i)) ? snapshot[i].state : "DISABLED");
    cJSON_AddNumberToObject(item,"lastSuccess",double(snapshot[i].lastSuccess));
  }
  cJSON* zones=cJSON_AddArrayToObject(response,"timeZones");
  for (const auto& zone : dashboard::kTimeZones) {
    cJSON* item=cJSON_CreateObject();
    cJSON_AddStringToObject(item,"id",zone.id);
    cJSON_AddStringToObject(item,"label",zone.label);
    cJSON_AddItemToArray(zones,item);
  }
  cJSON_AddBoolToObject(response, "connected", WiFi.status() == WL_CONNECTED);
  cJSON_AddStringToObject(response, "connectedSsid", WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "");
  cJSON_AddStringToObject(response, "ip", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "");
  cJSON_AddBoolToObject(response, "setupActive", apActive_.load());
  cJSON_AddBoolToObject(response, "updating", busy_.load());
  cJSON_AddBoolToObject(response, "restartPending", restart_.load());
  cJSON_AddStringToObject(response, "message", bootMessage_.c_str());
  cJSON_AddStringToObject(response, "csrfToken", csrf_.c_str());
  char* json = cJSON_PrintUnformatted(response);
  headers(); server_.send(200, "application/json", json ? json : "{}");
  cJSON_free(json); cJSON_Delete(response);
}
bool SettingsPortal::authorizeSettingsSave() {
  if (!authenticate()) return false;
  const String origin = server_.header("Origin");
  if (server_.header("X-CSRF-Token") != csrf_ || (origin.length() && origin != "http://" + server_.hostHeader())) {
    sendError(403, "Reload this page before saving."); return false;
  }
  if (restart_.load()) { sendError(409, "A settings change is already waiting for restart."); return false; }
  return true;
}
void SettingsPortal::saveSettings(const String& body) {
  Serial.println("SETTINGS SAVE: authenticated request received");
  if (!body.length() || body.length() > SettingsWebServer::kBodyLimit || body.indexOf("\\u0000") >= 0) {
    sendError(400, "Invalid settings request."); return;
  }
  const char* end = nullptr;
  cJSON* doc = cJSON_ParseWithLengthOpts(body.c_str(), body.length()+1, &end, true);
  if (!doc || !cJSON_IsObject(doc) || end != body.c_str()+body.length()) { cJSON_Delete(doc); sendError(400, "Invalid JSON settings."); return; }
  auto* networks = cJSON_GetObjectItemCaseSensitive(doc, "networks");
  auto* rotate180 = cJSON_GetObjectItemCaseSensitive(doc, "rotate180");
  auto* timeZone = cJSON_GetObjectItemCaseSensitive(doc, "timeZone");
  auto* dark = cJSON_GetObjectItemCaseSensitive(doc, "dark");
  auto* interval = cJSON_GetObjectItemCaseSensitive(doc, "intervalMinutes");
  auto* title = cJSON_GetObjectItemCaseSensitive(doc, "title");
  if (!cJSON_IsString(title) || strlen(title->valuestring) > dashboard::kMaxTitleLength) {
    cJSON_Delete(doc); sendError(400, "Enter a display title of 1-20 characters."); return;
  }
  if (!cJSON_IsBool(dark) || !cJSON_IsBool(rotate180) || !cJSON_IsString(timeZone) ||
      strlen(timeZone->valuestring) >= sizeof(config_.network.timeZone) || !dashboard::findTimeZone(timeZone->valuestring) ||
      !cJSON_IsNumber(interval) || !isfinite(interval->valuedouble) || floor(interval->valuedouble) != interval->valuedouble ||
      interval->valuedouble < 5 || interval->valuedouble > 1440) {
    cJSON_Delete(doc); sendError(400, "Check the display settings, time zone and interval (5-1440 minutes)."); return;
  }
  dashboard::DeviceConfig candidate=config_;
  candidate.title = title->valuestring;
  candidate.title.trim();
  candidate.network.dark = cJSON_IsTrue(dark);
  candidate.network.rotate180 = cJSON_IsTrue(rotate180);
  candidate.network.intervalMinutes = uint16_t(interval->valuedouble);
  strlcpy(candidate.network.timeZone, timeZone->valuestring, sizeof(candidate.network.timeZone));
  // Validate a copy first. The active runtime snapshot is never partly updated
  // by invalid fields or by an HTTP request arriving during a Google request.
  const char* error = dashboard::validateTitle(candidate.title);
  if (!error) error = dashboard::networksFromForm(networks, config_, candidate);
  if (!error) error=dashboard::connectionsFromForm(cJSON_GetObjectItemCaseSensitive(doc,"services"),config_,candidate);
  if (!error && dashboard::encodeConfig(candidate).length()>dashboard::kMaxConfigBytes)
    error="Combined settings exceed 4096 bytes. Remove unused credentials.";
  cJSON_Delete(doc);
  if (error) { sendError(400, error); return; }
  if (!dashboard::settings_store::stage(candidate)) { sendError(500, "Settings could not be saved. Existing settings were kept."); return; }
  // 202 confirms durable staging, not Wi-Fi acceptance. Boot promotes the
  // candidate, or keeps the previous active record if confirmation fails.
  Serial.println("SETTINGS SAVE: pending configuration written to NVS; restart scheduled");
  headers(); server_.send(202, "application/json", "{\"saved\":true,\"message\":\"Saved. Restarting after the current update finishes. Reconnect to your home Wi-Fi, then open seo-dashboard.local or the device IP.\"}");
  savedAt_.store(millis());
  restart_.store(true);
}

void SettingsPortal::publishService(dashboard::Service service, const String& state, int64_t lastSuccess) {
  if (service>=dashboard::ServiceCount) return;
  portENTER_CRITICAL(&statusMux_);
  strlcpy(serviceStatus_[service].state,state.c_str(),sizeof(serviceStatus_[service].state));
  serviceStatus_[service].lastSuccess=lastSuccess;
  portEXIT_CRITICAL(&statusMux_);
}

void SettingsPortal::publishDisplay(const dashboard::DisplayStatus& status) {
  // Copy the entire fixed-size snapshot together; no String allocations or HTTP work
  // may occur while interrupts are disabled by this critical section.
  portENTER_CRITICAL(&statusMux_);
  displayStatus_=status;
  portEXIT_CRITICAL(&statusMux_);
}
