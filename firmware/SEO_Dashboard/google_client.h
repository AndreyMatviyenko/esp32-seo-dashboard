#pragma once

#include "dashboard_data.h"
#include "json_helpers.h"

namespace dashboard {
enum class GoogleApi : uint8_t { OAuth, AdSense, GA4, GSC, CrUX };
struct GoogleResponse {
  Json document;
  Status status = Status::ApiError;
  int httpCode = 0;
};

// No shared bearer token or provider status. The caller passes authentication
// explicitly, and each response owns its JSON tree until it leaves scope.
class GoogleClient {
public:
  static String encode(const String& value);
  Status authorize(const Connection& connection, String& accessToken) const;
  GoogleResponse get(GoogleApi api, const String& url, const String& accessToken) const;
  GoogleResponse post(GoogleApi api, const String& url, const String& body,
                      const String& accessToken = "") const;
private:
  GoogleResponse request(GoogleApi api, const String& url, const String* body,
                         const String& accessToken) const;
};
}
