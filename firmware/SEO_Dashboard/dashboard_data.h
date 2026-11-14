#pragma once

#include <Arduino.h>
#include <time.h>
#include <math.h>
#include <type_traits>
#include "device_config.h"

namespace dashboard {

// Request health is separate from metric values: a healthy CrUX response may
// legitimately contain a FAIL assessment for the website.
enum class Status : uint8_t {
  Disabled, Waiting, Live, WifiError, ClockError, HttpsError, AuthRequired,
  AccessDenied, ApiError, JsonError, AuthError, AccountError, ApiWarning,
  NoData, DateMismatch, MemoryError, CwvDateError, TransferError, ResponseTooLarge
};
const char* statusLabel(Status status);
enum class OverallStatus : uint8_t { Setup, Live, Partial, Error };
const char* statusLabel(OverallStatus status);

enum class DisplayState : uint8_t { Waiting, Updating, Ready, Error };
enum class DisplayFrame : uint8_t { None, Dashboard, Setup };
enum class DashboardPage : uint8_t { Overview, AdSense, GA4, GSC, CWV };
const char* statusLabel(DisplayState state);
const char* frameLabel(DisplayFrame frame);

// Fixed-size, trivially copied snapshot for the HTTP task. lastSuccess belongs
// to this boot and advances only after a confirmed BUSY HIGH -> LOW refresh.
struct DisplayStatus {
  DisplayState state = DisplayState::Waiting;
  DisplayFrame frame = DisplayFrame::None;
  time_t lastSuccess = 0;
  char error[32] = {};
};
static_assert(std::is_trivially_copyable<DisplayStatus>::value,
              "DisplayStatus must remain safe to copy between tasks.");

struct ProviderState {
  Status status = Status::Waiting;
  time_t updated = 0;
};
struct AdSenseMetrics {
  String today = "--", month = "--", currency, day;
  // Independent groups keep a payment/history error from hiding current earnings.
  String yesterday = "--", last7 = "--", historyCurrency;
  String balance = "--", lastPayment = "--", lastPaymentDate;
  ProviderState history, payments;
};
struct Ga4Metrics {
  String realtimeUsers = "--", newUsers28 = "--";
  String totalUsers28 = "--", sessions28 = "--", views28 = "--";
  String engagementRate28 = "--"; // Display percentage, e.g. "62.4%".
};
struct GscMetrics {
  String clicks = "--", start, end;
  String impressions = "--", ctr = "--", position = "--";
};
struct CruxMetrics {
  String assessment = "--", firstDate, lastDate;
  // Original p75 units: milliseconds for LCP/INP, a unitless ratio for CLS.
  // A missing percentile stays NAN rather than becoming a reported zero.
  double lcp = NAN, inp = NAN, cls = NAN;
  bool detailsLoaded = false;
};

// Owned exclusively by the Arduino task. HTTP receives fixed-size status
// snapshots through SettingsPortal, never references to these mutable Strings.
struct DashboardData {
  AdSenseMetrics adsense;
  Ga4Metrics ga4;
  GscMetrics gsc;
  CruxMetrics crux;
  ProviderState providers[ServiceCount];
  DisplayStatus display;
  OverallStatus overall = OverallStatus::Setup;
};
}
