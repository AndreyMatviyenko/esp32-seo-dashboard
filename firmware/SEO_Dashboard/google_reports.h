#pragma once

#include "dashboard_data.h"

namespace dashboard {
// Each provider receives its own connection. A failed primary report leaves
// values intact; AdSense history/payments have independent status/cache groups.
Status fetchAdSense(const Connection& connection, AdSenseMetrics& metrics);
Status fetchGa4(const Connection& connection, Ga4Metrics& metrics);
Status fetchGsc(const Connection& connection, GscMetrics& metrics);
Status fetchCrux(const Connection& connection, CruxMetrics& metrics);
}
