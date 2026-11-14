#pragma once

#include "device_config.h"

namespace dashboard {
// Arduino-task only. Boot confirmation is bounded and synchronous; ordinary
// failover advances between loop iterations so HTTP and button handling continue.
class WifiNetworks {
public:
  bool join(const DeviceConfig& config);
  void startRetry();
  bool retrying() const { return retrying_; }
  // Returns true once after every configured network has failed this cycle.
  bool tick(const DeviceConfig& config);

private:
  size_t index_ = 0;
  uint32_t startedAt_ = 0;
  bool retrying_ = false, attempting_ = false;
};
}
