#pragma once

#include "dashboard_data.h"

namespace dashboard {
namespace display {
struct RefreshResult {
  bool confirmed;
  const char* error; // Static diagnostic identifier; copy it before the next attempt.
};
// Keep hardware SPI preparation before networking. No open SPI transaction is
// held while Google requests or HTTP settings requests run.
bool prepare();
RefreshResult showDashboard(const DeviceConfig& config, const DashboardData& data, const String& ipAddress);
RefreshResult showPage(const DeviceConfig& config, const DashboardData& data, DashboardPage page, const String& ipAddress);
RefreshResult showSetup(const DeviceConfig& config, const char* setupSsid, const char* setupPassword);
}
}
