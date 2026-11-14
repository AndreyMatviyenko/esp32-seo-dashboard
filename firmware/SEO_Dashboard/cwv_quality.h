#pragma once

#include <math.h>
#include <stdint.h>

namespace dashboard {
enum class CwvQuality : uint8_t { NoData, Good, NeedsWork, Poor };
struct CwvThresholds { double good, needsWork; };

// Official p75 thresholds, compared before display rounding. LCP/INP are in
// milliseconds; CLS is unitless. Both boundary values belong to the lower band.
// https://web.dev/articles/defining-core-web-vitals-thresholds
constexpr CwvThresholds kLcpThresholds = {2500, 4000};
constexpr CwvThresholds kInpThresholds = {200, 500};
constexpr CwvThresholds kClsThresholds = {0.1, 0.25};

inline CwvQuality cwvQuality(double value, CwvThresholds thresholds) {
  if (!isfinite(value) || value < 0) return CwvQuality::NoData;
  if (value <= thresholds.good) return CwvQuality::Good;
  if (value <= thresholds.needsWork) return CwvQuality::NeedsWork;
  return CwvQuality::Poor;
}

inline const char* qualityLabel(CwvQuality quality) {
  switch (quality) {
    case CwvQuality::Good: return "GOOD";
    case CwvQuality::NeedsWork: return "NEEDS WORK";
    case CwvQuality::Poor: return "POOR";
    case CwvQuality::NoData: return "NO DATA";
  }
  return "NO DATA";
}
}
