#include "dashboard_button.h"
#include "dashboard_pages.h"

namespace dashboard {
namespace {
constexpr uint32_t kDebounceMs = 35;
constexpr uint32_t kLongPressMs = 1200;
}

void DashboardButton::resetInput(uint32_t now, bool low) {
  rawLow_ = stableLow_ = low;
  rawSince_ = pressedAt_ = now;
  armed_ = !low; // Ignore BOOT held across startup until it has been released.
  longSent_ = false;
  home_ = false;
  next_ = 0;
}

void DashboardButton::begin() {
  pinMode(kPin, INPUT_PULLUP);
  resetInput(millis(), digitalRead(kPin) == LOW);
  attachInterruptArg(kPin, onEdge, this, CHANGE);
}

void ARDUINO_ISR_ATTR DashboardButton::onEdge(void* argument) {
  auto* self = static_cast<DashboardButton*>(argument);
  const Edge edge = {millis(), digitalRead(kPin) == LOW};
  portENTER_CRITICAL_ISR(&self->mux_);
  const uint8_t next = (self->write_ + 1) % kCapacity;
  if (next == self->read_) self->overflow_ = true;
  else { self->edges_[self->write_] = edge; self->write_ = next; }
  portEXIT_CRITICAL_ISR(&self->mux_);
}

void DashboardButton::settle(uint32_t now) {
  if (rawLow_ != stableLow_ && uint32_t(now - rawSince_) >= kDebounceMs) {
    stableLow_ = rawLow_;
    if (stableLow_) { pressedAt_ = rawSince_; longSent_ = false; }
    else if (!armed_) armed_ = true;
    else if (!longSent_) {
      if (uint32_t(rawSince_ - pressedAt_) >= kLongPressMs) { home_ = true; next_ = 0; }
      else ++next_;
    }
  }
  if (armed_ && stableLow_ && rawLow_ && !longSent_ &&
      uint32_t(now - pressedAt_) >= kLongPressMs) {
    home_ = true;
    next_ = 0;
    longSent_ = true;
  }
}

bool DashboardButton::update(const DeviceConfig& config) {
  Edge pending[kCapacity];
  uint8_t count = 0;
  bool overflow, low;
  uint32_t now;
  portENTER_CRITICAL(&mux_);
  while (read_ != write_) {
    pending[count++] = edges_[read_];
    read_ = (read_ + 1) % kCapacity;
  }
  overflow = overflow_; overflow_ = false;
  now = millis(); low = digitalRead(kPin) == LOW;
  portEXIT_CRITICAL(&mux_);
  if (overflow) {
    // Drop an ambiguous burst safely instead of inventing page changes.
    resetInput(now, low);
    Serial.println("BUTTON: edge buffer full; waiting for a clean press");
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    settle(pending[i].at);
    if (rawLow_ != pending[i].low) { rawLow_ = pending[i].low; rawSince_ = pending[i].at; }
  }
  settle(now);
  const DashboardPage before = page_;
  if (home_) page_ = DashboardPage::Overview;
  // Coalesce complete cycles and render only the final enabled page.
  const size_t pageCount = enabledPageCount(config); // Overview is always available.
  for (size_t i = next_ % pageCount; i; --i) page_ = nextDashboardPage(config, page_);
  home_ = false; next_ = 0;
  if (before != page_) Serial.printf("PAGE: %s (cached data)\n", pageLabel(page_));
  return before != page_;
}

void DashboardButton::discard() {
  portENTER_CRITICAL(&mux_);
  read_ = write_;
  overflow_ = false;
  resetInput(millis(), digitalRead(kPin) == LOW);
  portEXIT_CRITICAL(&mux_);
}
}
