#include "settings_http.h"
#include <errno.h>
#include <lwip/sockets.h>
#include <new>
#include <utility>

namespace {
// Drain only a bounded amount/time after sending an error. Closing immediately
// with unread input can reset the connection and discard a Digest challenge.
void closeRequest(NetworkClient& client) {
  uint8_t discard[128];
  size_t drained = 0;
  const uint32_t started = millis();
  while (drained < SettingsWebServer::kBodyLimit * 2 && uint32_t(millis() - started) < 50) {
    const int available = client.available();
    if (available <= 0) { delay(1); continue; }
    const int read = client.read(discard, min(size_t(available), sizeof(discard)));
    if (read <= 0) break;
    drained += size_t(read);
  }
  client.setTimeout(1); // Let the core raw reader abort immediately after close.
  client.stop();
}

class SettingsBodyHandler final : public RequestHandler {
public:
  SettingsBodyHandler(std::function<bool()> authorize, std::function<void(const String&)> save,
                      std::function<void(int, const char*)> error)
    : authorize_(std::move(authorize)), save_(std::move(save)), error_(std::move(error)) {}
  bool canHandle(WebServer&, HTTPMethod method, const String& uri) override {
    return method == HTTP_POST && uri == "/api/settings";
  }
  bool canRaw(WebServer&, const String&) override { return true; }
  void raw(WebServer& server, const String&, HTTPRaw& raw) override {
    if (raw.status == RAW_START) {
      releaseBody();
      if (!authorize_()) { closeRequest(server.client()); return; }
      const int declared = server.clientContentLength();
      if (declared <= 0 || size_t(declared) > SettingsWebServer::kBodyLimit) {
        fail(server, 413, "Settings request is too large."); return;
      }
      expected_ = size_t(declared);
      if (!body_.reserve(expected_)) {
        fail(server, 503, "Not enough memory to receive settings."); return;
      }
      receiveBody(server, raw);
    } else if (raw.status == RAW_END) {
      complete_ = expected_ && body_.length() == expected_;
    } else if (raw.status == RAW_ABORTED) releaseBody();
  }
  bool handle(WebServer&, HTTPMethod, const String&) override {
    if (complete_) save_(body_);
    else error_(400, "Incomplete settings request.");
    releaseBody();
    return true;
  }
private:
  void releaseBody() {
    // Assigning an empty literal retains String's heap capacity on this core.
    // A null source invalidates it and releases the request buffer instead.
    body_ = static_cast<const char*>(nullptr);
    expected_ = 0; complete_ = false;
  }
  void receiveBody(WebServer& server, HTTPRaw& raw) {
    // Core readBytes() resets its timeout whenever a byte arrives. Read here
    // without blocking so a trickling client cannot extend the total deadline.
    // Advancing the core's received-byte counter skips its normal body loop;
    // it still delivers RAW_END and routes the completed request as usual.
    NetworkClient& client = server.client();
    const uint32_t started = millis();
    while (raw.totalSize < expected_) {
      if (uint32_t(millis() - started) >= 10000) {
        fail(server, 408, "Settings request timed out."); return;
      }
      const int available = client.available();
      if (available > 0) {
        const size_t length = min(min(size_t(available), sizeof(raw.buf)), expected_ - raw.totalSize);
        const int received = client.read(raw.buf, length);
        if (received > 0) {
          if (memchr(raw.buf, 0, received)) { fail(server, 400, "Invalid settings request."); return; }
          if (!body_.concat(reinterpret_cast<const char*>(raw.buf), received)) {
            fail(server, 503, "Not enough memory to receive settings."); return;
          }
          raw.totalSize += size_t(received);
          continue;
        }
      }
      if (!client.connected()) { fail(server, 400, "Incomplete settings request."); return; }
      delay(2);
    }
  }
  void fail(WebServer& server, int status, const char* message) {
    error_(status, message);
    releaseBody();
    closeRequest(server.client());
  }
  std::function<bool()> authorize_;
  std::function<void(const String&)> save_;
  std::function<void(int, const char*)> error_;
  String body_;
  size_t expected_ = 0;
  bool complete_ = false;
};
}

void SettingsWebServer::rejectRequest(int status, const char* message) {
  // Messages are fixed application strings; never echo request headers/body.
  char body[160];
  const int formatted = snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
  const size_t length = min(size_t(max(formatted, 0)), sizeof(body) - 1);
  _currentClient.printf("HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
    status, responseCodeToString(status).c_str(), int(length));
  _currentClient.write(reinterpret_cast<const uint8_t*>(body), length);
  closeRequest(_currentClient);
  _currentClient = NetworkClient();
  _currentStatus = HC_NONE;
  headersChecked_ = false;
}

bool SettingsWebServer::checkHeaders() {
  // Both parsers must see the same field boundaries. Reject bare CR/LF and
  // control bytes before splitting; a hidden Content-Type must not bypass
  // the JSON-only check and enter the core's multipart parser instead.
  for (const char* cursor = headerProbe_; *cursor; ++cursor) {
    if (*cursor == '\r' && cursor[1] == '\n') { ++cursor; continue; }
    const uint8_t byte = uint8_t(*cursor);
    if ((byte < 32 && byte != '\t') || byte == 127) {
      rejectRequest(400, "Invalid request headers."); return false;
    }
  }
  char* lineEnd = strstr(headerProbe_, "\r\n");
  if (!lineEnd) { rejectRequest(400, "Invalid request line."); return false; }
  *lineEnd = 0;
  const String requestLine(headerProbe_);
  const int firstSpace = requestLine.indexOf(' '), lastSpace = requestLine.lastIndexOf(' ');
  if (firstSpace <= 0 || lastSpace <= firstSpace) { rejectRequest(400, "Invalid request line."); return false; }
  const String method = requestLine.substring(0, firstSpace);
  const String target = requestLine.substring(firstSpace + 1, lastSpace);
  const bool post = method == "POST";
  if (!post && method != "GET" && method != "HEAD") { rejectRequest(405, "Method not supported."); return false; }
  if (post && target != "/api/settings") { rejectRequest(404, "Not found."); return false; }
  bool lengthSeen = false, typeSeen = false;
  size_t length = 0;
  char* line = lineEnd + 2;
  while (*line) {
    lineEnd = strstr(line, "\r\n");
    if (!lineEnd) { rejectRequest(400, "Invalid request headers."); return false; }
    *lineEnd = 0;
    char* colon = strchr(line, ':');
    if (!colon) { rejectRequest(400, "Invalid request headers."); return false; }
    *colon = 0;
    String name(line), value(colon + 1); name.trim(); value.trim();
    if (name.equalsIgnoreCase("Transfer-Encoding")) { rejectRequest(400, "Transfer-Encoding is not supported."); return false; }
    if (name.equalsIgnoreCase("Content-Length")) {
      if (lengthSeen || !value.length()) { rejectRequest(400, "Invalid Content-Length."); return false; }
      lengthSeen = true;
      for (size_t i = 0; i < value.length(); ++i) {
        if (!isDigit(value[i])) { rejectRequest(400, "Invalid Content-Length."); return false; }
        const size_t digit = size_t(value[i] - '0');
        if (length > (kBodyLimit - digit) / 10) { rejectRequest(413, "Settings request is too large."); return false; }
        length = length * 10 + digit;
      }
    } else if (name.equalsIgnoreCase("Content-Type")) {
      if (typeSeen) { rejectRequest(400, "Duplicate Content-Type."); return false; }
      typeSeen = true;
      const int semicolon = value.indexOf(';');
      if (semicolon >= 0) value.remove(semicolon);
      value.trim();
      if (post && !value.equalsIgnoreCase("application/json")) { rejectRequest(415, "Send JSON settings."); return false; }
    }
    line = lineEnd + 2;
  }
  if (post && (!lengthSeen || !length)) { rejectRequest(411, "Content-Length is required."); return false; }
  if (post && !typeSeen) { rejectRequest(415, "Send JSON settings."); return false; }
  if (!post && length) { rejectRequest(400, "This method does not accept a body."); return false; }
  return true;
}

void SettingsWebServer::handleClient() {
  // Core 3.3.12 accepts one fresh socket per request. Peek before its first
  // NetworkClient read: bytes remain available to the normal header parser.
  if (_currentStatus == HC_NONE) {
    _currentClient = _server.accept();
    if (!_currentClient) return;
    _currentStatus = HC_WAIT_READ; _statusChange = millis(); headersChecked_ = false;
  }
  if (_currentStatus == HC_WAIT_READ && !headersChecked_) {
    if (uint32_t(millis() - _statusChange) >= 5000) {
      rejectRequest(408, "Request headers timed out."); return;
    }
    const int received = recv(_currentClient.fd(), headerProbe_, kHeaderLimit, MSG_PEEK | MSG_DONTWAIT);
    if (received > 0) {
      headerProbe_[received] = 0;
      char* end = strstr(headerProbe_, "\r\n\r\n");
      if (end) {
        end[2] = 0; // Keep the final header's CRLF, exclude the body/blank line.
        const bool accepted = checkHeaders();
        memset(headerProbe_, 0, sizeof(headerProbe_));
        if (!accepted) return;
        headersChecked_ = true;
      } else if (size_t(received) == kHeaderLimit) {
        memset(headerProbe_, 0, sizeof(headerProbe_));
        rejectRequest(431, "Request headers are too large."); return;
      }
    } else if (!received || (errno != EAGAIN && errno != EWOULDBLOCK)) {
      _currentClient.stop(); _currentClient = NetworkClient(); _currentStatus = HC_NONE;
      return;
    }
    if (!headersChecked_) {
      memset(headerProbe_, 0, sizeof(headerProbe_));
      return;
    }
  }
  WebServer::handleClient();
}

bool SettingsWebServer::onSettingsSave(std::function<bool()> authorize,
                                     std::function<void(const String&)> save,
                                     std::function<void(int, const char*)> error) {
  auto earlyAuthorize = [this, authorize]() {
    // RAW_START precedes the core's normal per-request response reset.
    _contentLength = CONTENT_LENGTH_NOT_SET; _responseCode = 0; _clearResponseHeaders();
    return authorize();
  };
  auto* handler = new (std::nothrow) SettingsBodyHandler(std::move(earlyAuthorize), std::move(save), std::move(error));
  if (!handler) return false;
  addHandler(handler);
  return true;
}

void SettingsWebServer::requestSetupAuthentication(bool api) {
  // Keep a stable per-boot challenge for parallel browser requests on iOS.
  if (!_snonce.length()) { _snonce = _getRandomHexString(); _sopaque = _getRandomHexString(); _srealm = "SEO Dashboard"; }
  sendHeader("WWW-Authenticate", "Digest realm=\"" + _srealm +
    "\", qop=\"auth\", nonce=\"" + _snonce + "\", opaque=\"" + _sopaque + "\"");
  if (api) send(401, "application/json", "{\"error\":\"Sign in as admin, then reload the settings page.\"}");
  else send(401, "text/html; charset=utf-8", "Use admin and the password shown on the e-paper setup screen. Open this address in Safari if the Wi-Fi login window does not show a login dialog.");
}
