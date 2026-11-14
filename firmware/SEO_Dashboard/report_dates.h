#pragma once

#include <Arduino.h>
#include "cJSON.h"

namespace dashboard {
// Strict Gregorian date from Google's {year, month, day}, or an empty String.
String calendarDate(const cJSON* date, int minimumYear = 2000);
}
