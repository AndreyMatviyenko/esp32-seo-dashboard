#pragma once

#include <Preferences.h>
#include "dashboard_data.h"

namespace dashboard {
// Arduino-task only. Each provider's values, timestamp, and source fingerprint
// are committed as one NVS blob, preventing mixed reports after a power loss.
class MetricsCache {
public:
  bool begin();
  void load(const DeviceConfig& config, DashboardData& data);
  bool save(Service service, const Connection& connection, const DashboardData& data);
private:
  Preferences store_;
  bool available_ = false;
  bool loadRecord(Service service, const String& source, DashboardData& data);
  bool loadLegacy(Service service, const String& source, DashboardData& data);
};
}
