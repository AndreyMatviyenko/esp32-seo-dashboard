#pragma once

#include "device_config.h"

namespace dashboard {
namespace settings_store {
// Records contain private values and must never be returned as HTTP responses.
// Boot owns active/pending promotion; the HTTP worker may only stage a candidate.
bool begin();
bool loadActive(DeviceConfig& value);
bool loadPending(DeviceConfig& value);
bool saveActive(const DeviceConfig& value);
bool stage(const DeviceConfig& value);
bool discardPending();
}
}
