#pragma once

#include "dashboard_data.h"

namespace dashboard {
const char* pageLabel(DashboardPage page);
Service pageService(DashboardPage page); // ServiceCount denotes Overview/unknown.
bool pageEnabled(const DeviceConfig& config, DashboardPage page);
size_t enabledPageCount(const DeviceConfig& config);
DashboardPage nextDashboardPage(const DeviceConfig& config, DashboardPage page);
}
