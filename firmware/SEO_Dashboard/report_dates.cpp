#include "report_dates.h"

namespace dashboard {
String calendarDate(const cJSON* date, int minimumYear) {
  const cJSON* year = cJSON_GetObjectItemCaseSensitive(date, "year");
  const cJSON* month = cJSON_GetObjectItemCaseSensitive(date, "month");
  const cJSON* day = cJSON_GetObjectItemCaseSensitive(date, "day");
  if (!cJSON_IsNumber(year) || !cJSON_IsNumber(month) || !cJSON_IsNumber(day)) return "";
  const int y = year->valueint, m = month->valueint, d = day->valueint;
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (y < minimumYear || y > 9999 || m < 1 || m > 12 || d < 1 ||
      d > days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ||
      year->valuedouble != y || month->valuedouble != m || day->valuedouble != d) return "";
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", y, m, d);
  return buffer;
}
}
