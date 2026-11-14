#include "google_client.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <StreamString.h>

namespace dashboard {
namespace {
constexpr size_t kReportLimit = 32 * 1024;
constexpr size_t kOAuthLimit = 8 * 1024;
constexpr size_t kErrorLimit = 8 * 1024;

// HTTPClient owns HTTP framing; this sink bounds decoded payload bytes even
// with chunked encoding or no Content-Length. A failed write stays failed
// because HTTPClient may retry it before reporting a stream-write error.
class LimitedBody : public StreamString {
public:
  explicit LimitedBody(size_t limit) : limit_(limit) {}
  size_t write(const uint8_t* buffer, size_t size) override {
    if (tooLarge_ || noMemory_) return 0;
    if (size > limit_ - length()) {
      tooLarge_ = true;
      setWriteError(1);
      return 0;
    }
    const size_t written = StreamString::write(buffer, size);
    if (written != size) { noMemory_ = true; setWriteError(1); }
    return written;
  }
  size_t write(uint8_t value) override {
    return write(&value, 1);
  }
  size_t limit() const { return limit_; }
  bool tooLarge() const { return tooLarge_; }
  bool noMemory() const { return noMemory_; }
private:
  size_t limit_;
  bool tooLarge_ = false, noMemory_ = false;
};

Status readBody(HTTPClient& http, LimitedBody& body) {
  const int declared = http.getSize();
  if (declared >= 0 && size_t(declared) > body.limit()) return Status::ResponseTooLarge;
  if (declared > 0 && !body.reserve(size_t(declared) + 1)) return Status::MemoryError;
  const int received = http.writeToStream(&body);
  if (body.tooLarge()) return Status::ResponseTooLarge;
  if (body.noMemory()) return Status::MemoryError;
  // writeToStream checks fixed-length bodies and chunk framing. Check its
  // return value, not just JSON syntax: a truncated transfer may contain a
  // valid JSON prefix. getSize() also supplies the decoded total after chunking.
  const int framed = http.getSize();
  if (received < 0 || size_t(received) != body.length() ||
      (declared >= 0 && received != declared) || (framed >= 0 && received != framed))
    return Status::TransferError;
  return Status::Live;
}

const char* requestLabel(GoogleApi api) {
  switch (api) {
    case GoogleApi::OAuth: return "OAuth token";
    case GoogleApi::AdSense: return "AdSense report/accounts";
    case GoogleApi::GA4: return "GA4 report";
    case GoogleApi::GSC: return "GSC report";
    case GoogleApi::CrUX: return "CrUX mobile";
  }
  return "Google";
}

// Log only recognized identifiers, never Google's free-form error message or
// raw response: those may contain account details or request parameters.
const char* knownReason(const String& reason) {
  const char* allowed[] = {
    "invalid_grant", "invalid_client", "unauthorized_client", "access_denied",
    "insufficientPermissions", "forbidden", "accessNotConfigured",
    "SERVICE_DISABLED", "ACCESS_TOKEN_SCOPE_INSUFFICIENT", "PERMISSION_DENIED",
    "quotaExceeded", "rateLimitExceeded", "userRateLimitExceeded",
    "dailyLimitExceeded", "RESOURCE_EXHAUSTED", "NOT_FOUND", "INVALID_ARGUMENT"
  };
  for (const char* value : allowed) if (reason == value) return value;
  return nullptr;
}
void logReason(const String& reason) {
  if (const char* safe = knownReason(reason)) Serial.printf("Google error reason: %s\n", safe);
}
void inspectError(const String& body, Status& status) {
  Json doc(cJSON_Parse(body.c_str()));
  const cJSON* error = cJSON_GetObjectItemCaseSensitive(doc.get(), "error");
  if (cJSON_IsString(error)) {
    const String reason(error->valuestring);
    logReason(reason);
    if (reason == "invalid_grant" || reason == "invalid_client") status = Status::AuthRequired;
    return;
  }
  logReason(jsonText(error, "status"));
  const cJSON* errors = cJSON_GetObjectItemCaseSensitive(error, "errors");
  for (int i = 0; i < cJSON_GetArraySize(errors); ++i)
    logReason(jsonText(cJSON_GetArrayItem(errors, i), "reason"));
  const cJSON* details = cJSON_GetObjectItemCaseSensitive(error, "details");
  for (int i = 0; i < cJSON_GetArraySize(details); ++i)
    logReason(jsonText(cJSON_GetArrayItem(details, i), "reason"));
}
}

String GoogleClient::encode(const String& value) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = value[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

GoogleResponse GoogleClient::request(GoogleApi api, const String& url, const String* body,
                                     const String& accessToken) const {
  const char* label = requestLabel(api);
  Serial.printf("Request START: %s; uptime=%lu ms; heap=%u; minHeap=%u; RSSI=%d\n",
    label, (unsigned long)millis(), ESP.getFreeHeap(), ESP.getMinFreeHeap(), WiFi.RSSI());
  Serial.flush();
  const uint32_t started = millis();
  WiFiClientSecure client;
  client.useBuiltinCACertBundle();
  client.setHandshakeTimeout(20);
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  if (!http.begin(client, url)) return {Json{}, Status::HttpsError, 0};
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (body) http.addHeader("Content-Type", api == GoogleApi::OAuth ?
    "application/x-www-form-urlencoded" : "application/json");
  if (accessToken.length()) http.addHeader("Authorization", "Bearer " + accessToken);
  const int code = body ? http.POST(*body) : http.GET();
  // Missing field data is handled by the CrUX provider, not synthesized as an
  // API failure. No URL logging: CrUX carries its key in a query parameter.
  if (code == 404 && api == GoogleApi::CrUX) {
    http.end();
    return {Json{}, Status::NoData, code};
  }
  if (code != 200) {
    Status status = code == 401 ? Status::AuthRequired :
      code == 403 ? Status::AccessDenied : Status::ApiError;
    Serial.printf("Request FAILED: %s; code=%d; elapsed=%lu ms; WiFi=%d; heap=%u\n",
      label, code, (unsigned long)(millis() - started), int(WiFi.status()), ESP.getFreeHeap());
    if (code < 0) {
      char detail[160] = {};
      const int tlsCode = client.lastError(detail, sizeof(detail));
      Serial.printf("Transport: %s; TLS code=%d; TLS detail=%s\n",
        HTTPClient::errorToString(code).c_str(), tlsCode, detail);
    } else {
      LimitedBody errorBody(kErrorLimit);
      const Status read = readBody(http, errorBody);
      if (read == Status::Live) inspectError(errorBody, status);
      else Serial.printf("Error details skipped: %s; %s\n", label, statusLabel(read));
    }
    http.end();
    return {Json{}, status, code};
  }
  LimitedBody responseBody(api == GoogleApi::OAuth ? kOAuthLimit : kReportLimit);
  const Status read = readBody(http, responseBody);
  http.end();
  if (read != Status::Live) {
    Serial.printf("Response rejected: %s; %s; received=%u; limit=%u\n", label,
      statusLabel(read), unsigned(responseBody.length()), unsigned(responseBody.limit()));
    return {Json{}, read, code};
  }
  Serial.printf("Request OK: %s; elapsed=%lu ms; heap=%u\n", label,
    (unsigned long)(millis() - started), ESP.getFreeHeap());
  const char* end = nullptr;
  Json document(cJSON_ParseWithLengthOpts(responseBody.c_str(), responseBody.length() + 1, &end, true));
  const Status status = document && end == responseBody.c_str() + responseBody.length() ?
    Status::Live : Status::JsonError;
  if (status != Status::Live) document.reset();
  return {std::move(document), status, code};
}

GoogleResponse GoogleClient::get(GoogleApi api, const String& url, const String& accessToken) const {
  return request(api, url, nullptr, accessToken);
}
GoogleResponse GoogleClient::post(GoogleApi api, const String& url, const String& body,
                                 const String& accessToken) const {
  return request(api, url, &body, accessToken);
}
Status GoogleClient::authorize(const Connection& connection, String& accessToken) const {
  accessToken = "";
  const String form = "grant_type=refresh_token&client_id=" + encode(connection.clientId)
    + "&client_secret=" + encode(connection.clientSecret)
    + "&refresh_token=" + encode(connection.refreshToken);
  auto response = post(GoogleApi::OAuth, "https://oauth2.googleapis.com/token", form);
  if (response.status != Status::Live) return response.status;
  accessToken = jsonText(response.document.get(), "access_token");
  return accessToken.length() ? Status::Live : Status::AuthError;
}
}
