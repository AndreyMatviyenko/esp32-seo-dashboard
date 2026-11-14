#pragma once

#include "dashboard_data.h"

namespace dashboard {
// Edges are captured by an ISR; only the Arduino task interprets gestures and
// changes the selected page. No network, allocation, or SPI work runs in the ISR.
class DashboardButton {
 public:
  void begin();
  bool update(const DeviceConfig& config);
  void discard();
  DashboardPage page() const { return page_; }

 private:
  static constexpr uint8_t kPin = 9; // Active-low, built-in ESP32-C3 BOOT button.
  static constexpr uint8_t kCapacity = 64;
  struct Edge { uint32_t at; bool low; };
  static void ARDUINO_ISR_ATTR onEdge(void* argument);
  void settle(uint32_t now);
  void resetInput(uint32_t now, bool low);
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Edge edges_[kCapacity] = {};
  uint8_t read_ = 0, write_ = 0;
  bool overflow_ = false, rawLow_ = false, stableLow_ = false;
  bool armed_ = false, longSent_ = false, home_ = false;
  uint32_t rawSince_ = 0, pressedAt_ = 0, next_ = 0;
  DashboardPage page_ = DashboardPage::Overview;
};
}
