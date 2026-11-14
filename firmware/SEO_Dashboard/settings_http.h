#pragma once

#include <WebServer.h>
#include <functional>

// The core server still owns routing and Digest authentication. This adapter
// checks fresh socket headers before WebServer can buffer a request body.
class SettingsWebServer : public WebServer {
public:
  using WebServer::WebServer;
  static constexpr size_t kBodyLimit = 8192;
  void handleClient() override;
  void requestSetupAuthentication(bool api);
  bool onSettingsSave(std::function<bool()> authorize,
                      std::function<void(const String&)> save,
                      std::function<void(int, const char*)> error);

private:
  static constexpr size_t kHeaderLimit = 4096;
  char headerProbe_[kHeaderLimit + 1] = {};
  bool headersChecked_ = false;
  bool checkHeaders();
  void rejectRequest(int status, const char* message);
};
