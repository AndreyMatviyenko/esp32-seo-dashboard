#include "google_reports.h"
#include "google_client.h"
#include "cwv_quality.h"
#include "report_dates.h"
#include <math.h>

namespace dashboard {
namespace {
String reportDate(const cJSON* doc) {
  return calendarDate(cJSON_GetObjectItemCaseSensitive(doc, "endDate"));
}

Status findAccount(const GoogleClient& client, const String& token, String& account) {
  auto response = client.get(GoogleApi::AdSense, "https://adsense.googleapis.com/v2/accounts", token);
  if (response.status != Status::Live) return response.status;
  const cJSON* doc = response.document.get();
  const cJSON* accounts = cJSON_GetObjectItemCaseSensitive(doc, "accounts");
  // Reject ambiguity instead of choosing the first account in a paginated list.
  if (cJSON_GetArraySize(accounts) != 1 || jsonText(doc, "nextPageToken").length()) return Status::AccountError;
  account = jsonText(cJSON_GetArrayItem(accounts, 0), "name");
  if (!account.startsWith("accounts/pub-")) return Status::AccountError;
  for (size_t i = 13; i < account.length(); ++i)
    if (!isDigit(account[i])) return Status::AccountError;
  return Status::Live;
}

String previousDay(const String& day) {
  if (day.length() != 10) return "";
  struct tm date = {};
  date.tm_year = day.substring(0, 4).toInt() - 1900;
  date.tm_mon = day.substring(5, 7).toInt() - 1;
  date.tm_mday = day.substring(8, 10).toInt() - 1;
  date.tm_hour = 12;
  // The Arduino task keeps TZ=UTC outside explicitly scoped date conversions.
  const time_t stamp = mktime(&date);
  if (stamp < 0) return "";
  char buffer[16]; strftime(buffer, sizeof(buffer), "%Y-%m-%d", &date);
  return buffer;
}

String paymentAmount(String amount) {
  // Payments return formatted strings, not a numeric amount/currency pair.
  // Preserve separators and currency; map common UTF-8 symbols for ASCII fonts.
  if (amount.length() > 96) return "";
  const char* symbols[] = {"€", "£", "¥", "₴", "₽", "₹", "₩", "\xC2\xA0", "\xE2\x80\xAF"};
  const char* ascii[] = {"EUR ", "GBP ", "JPY ", "UAH ", "RUB ", "INR ", "KRW ", " ", " "};
  for (int i = 0; i < 9; ++i) amount.replace(symbols[i], ascii[i]);
  amount.trim();
  bool digit = false;
  if (amount.length() > 64) return "";
  for (size_t i = 0; i < amount.length(); ++i) {
    const unsigned char c = amount[i];
    if (c < 32 || c > 126) return "";
    if (isDigit(c)) digit = true;
  }
  return digit ? amount : "";
}

Status payments(const GoogleClient& client, const String& token, const String& account,
                String& balance, String& lastAmount, String& lastDate) {
  auto response = client.get(GoogleApi::AdSense,
    "https://adsense.googleapis.com/v2/" + account + "/payments", token);
  if (response.status != Status::Live) return response.status;
  const cJSON* list = cJSON_GetObjectItemCaseSensitive(response.document.get(), "payments");
  if (list && !cJSON_IsArray(list)) return Status::JsonError;
  String unpaid = "--", paid = "--", date;
  const String prefix = account + "/payments/";
  for (int i = 0; i < cJSON_GetArraySize(list); ++i) {
    const cJSON* payment = cJSON_GetArrayItem(list, i);
    const String name = jsonText(payment, "name");
    if (!name.startsWith(prefix)) continue;
    const String id = name.substring(prefix.length());
    if (id.startsWith("youtube-")) continue; // Separate YouTube balances/payments.
    if (id != "unpaid" && id.length() != 10) continue;
    const String amount = paymentAmount(jsonText(payment, "amount"));
    if (!amount.length()) return Status::NoData;
    if (id == "unpaid") unpaid = amount;
    else {
      const String paidOn = calendarDate(cJSON_GetObjectItemCaseSensitive(payment, "date"));
      if (!paidOn.length() || paidOn != id) return Status::DateMismatch;
      // API array order is not a contract. ISO dates sort chronologically.
      if (!date.length() || paidOn.compareTo(date) > 0) { date = paidOn; paid = amount; }
    }
  }
  balance = unpaid; lastAmount = paid; lastDate = date;
  return Status::Live;
}

Status earnings(const GoogleClient& client, const String& token, const String& account,
                const char* range, String& amount, String& currency, String& day) {
  const String url = "https://adsense.googleapis.com/v2/" + account
    + "/reports:generate?metrics=ESTIMATED_EARNINGS&reportingTimeZone=ACCOUNT_TIME_ZONE&dateRange=" + range;
  auto response = client.get(GoogleApi::AdSense, url, token);
  if (response.status != Status::Live) return response.status;
  const cJSON* doc = response.document.get();
  const cJSON* headers = cJSON_GetObjectItemCaseSensitive(doc, "headers");
  int index = -1;
  for (int i = 0; i < cJSON_GetArraySize(headers); ++i)
    if (jsonText(cJSON_GetArrayItem(headers, i), "name") == "ESTIMATED_EARNINGS") index = i;
  const cJSON* totals = cJSON_GetObjectItemCaseSensitive(doc, "totals");
  const cJSON* cells = cJSON_GetObjectItemCaseSensitive(totals, "cells");
  if (!cJSON_IsArray(cells)) {
    const cJSON* rows = cJSON_GetObjectItemCaseSensitive(doc, "rows");
    if (cJSON_GetArraySize(rows) == 1) cells = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(rows, 0), "cells");
  }
  if (cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(doc, "warnings")) > 0) return Status::ApiWarning;
  if (index < 0 || cJSON_GetArraySize(cells) <= index) return Status::NoData;
  const String value = jsonText(cJSON_GetArrayItem(cells, index), "value");
  char* end = nullptr;
  const double number = strtod(value.c_str(), &end);
  currency = jsonText(cJSON_GetArrayItem(headers, index), "currencyCode");
  day = reportDate(doc);
  if (!value.length() || !end || *end || !isfinite(number) || currency.length() != 3 || !day.length())
    return Status::NoData;
  amount = String(number, 2);
  return Status::Live;
}

Status gaReport(const GoogleClient& client, const String& token, const String& property,
                bool realtime, String (&result)[5]) {
  // All five historical metrics share one period and one response. Realtime
  // remains a separate request because its endpoint/schema is different.
  const String body = realtime
    ? R"({"metrics":[{"name":"activeUsers"}],"minuteRanges":[{"startMinutesAgo":29,"endMinutesAgo":0}]})"
    : R"({"metrics":[{"name":"newUsers"},{"name":"totalUsers"},{"name":"sessions"},{"name":"screenPageViews"},{"name":"engagementRate"}],"dateRanges":[{"startDate":"28daysAgo","endDate":"yesterday"}]})";
  const String url = "https://analyticsdata.googleapis.com/v1beta/properties/" + property
    + (realtime ? ":runRealtimeReport" : ":runReport");
  auto response = client.post(GoogleApi::GA4, url, body, token);
  if (response.status != Status::Live) return response.status;
  const cJSON* doc = response.document.get();
  const cJSON* headers = cJSON_GetObjectItemCaseSensitive(doc, "metricHeaders");
  const cJSON* rows = cJSON_GetObjectItemCaseSensitive(doc, "rows");
  const cJSON* metadata = cJSON_GetObjectItemCaseSensitive(doc, "metadata");
  if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(metadata, "subjectToThresholding")) ||
      cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(metadata, "dataLossFromOtherRow")) ||
      cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(metadata, "samplingMetadatas")) > 0)
    return Status::ApiWarning;
  const int count = realtime ? 1 : 5;
  const char* names[] = {realtime ? "activeUsers" : "newUsers", "totalUsers",
    "sessions", "screenPageViews", "engagementRate"};
  if (!cJSON_IsArray(headers) || cJSON_GetArraySize(headers) != count) return Status::NoData;
  int order[5];
  bool seen[5] = {};
  for (int i = 0; i < count; ++i) {
    const String name = jsonText(cJSON_GetArrayItem(headers, i), "name");
    int index = -1;
    for (int j = 0; j < count; ++j) if (name == names[j]) index = j;
    if (index < 0 || seen[index]) return Status::NoData;
    seen[index] = true; order[i] = index;
  }
  const cJSON* rowCount = cJSON_GetObjectItemCaseSensitive(doc, "rowCount");
  if (!rows || (cJSON_IsArray(rows) && cJSON_GetArraySize(rows) == 0)) {
    if (rowCount && (!cJSON_IsNumber(rowCount) || rowCount->valuedouble != 0)) return Status::NoData;
    for (int i = 0; i < count; ++i) result[i] = i == 4 ? "--" : "0";
    // No sessions means no observed engagement ratio; do not invent 0%.
    return Status::Live;
  }
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) != 1 ||
      (rowCount && (!cJSON_IsNumber(rowCount) || rowCount->valuedouble != 1))) return Status::NoData;
  const cJSON* values = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(rows, 0), "metricValues");
  if (!cJSON_IsArray(values) || cJSON_GetArraySize(values) != count) return Status::NoData;
  for (int i = 0; i < count; ++i) {
    const String value = jsonText(cJSON_GetArrayItem(values, i), "value");
    if (!value.length() || value.length() > 32) return Status::NoData;
    const int index = order[i];
    if (index == 4) {
      char* end = nullptr;
      const double ratio = strtod(value.c_str(), &end);
      if (!end || end == value.c_str() || *end || !isfinite(ratio) || ratio < 0 || ratio > 1) return Status::NoData;
      result[index] = String(ratio * 100, 1) + "%";
    } else {
      for (size_t j = 0; j < value.length(); ++j) if (!isDigit(value[j])) return Status::NoData;
      result[index] = value;
    }
  }
  return Status::Live;
}

void gscPeriod(String& start, String& end) {
  // GSC uses Pacific calendar days. Noon calendar arithmetic handles US DST;
  // both endpoints count toward 28 days, ending yesterday (dataState=all).
  // TZ is process-wide: only the Arduino task converts dates, then restores UTC.
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1); tzset();
  const time_t now = time(nullptr);
  struct tm date; localtime_r(&now, &date);
  date.tm_hour = 12; date.tm_min = 0; date.tm_sec = 0; date.tm_mday -= 1; date.tm_isdst = -1;
  mktime(&date);
  char buffer[16]; strftime(buffer, sizeof(buffer), "%Y-%m-%d", &date); end = buffer;
  date.tm_mday -= 27; date.tm_isdst = -1; mktime(&date);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &date); start = buffer;
  setenv("TZ", "UTC0", 1); tzset();
}
}

Status fetchAdSense(const Connection& connection, AdSenseMetrics& metrics) {
  const GoogleClient client;
  String token, account;
  Status status = client.authorize(connection, token);
  if (status != Status::Live) return status;
  status = findAccount(client, token, account);
  if (status != Status::Live) return status;
  AdSenseMetrics candidate = metrics;
  String monthCurrency, monthDay;
  status = earnings(client, token, account, "TODAY", candidate.today, candidate.currency, candidate.day);
  if (status != Status::Live) return status;
  status = earnings(client, token, account, "MONTH_TO_DATE", candidate.month, monthCurrency, monthDay);
  if (status != Status::Live) return status;
  // Publish together: midnight between calls must not mix account dates/currency.
  if (candidate.currency != monthCurrency || candidate.day != monthDay) return Status::DateMismatch;
  String yesterday, last7, yesterdayCurrency, last7Currency, yesterdayDate, last7Date;
  candidate.history.status = earnings(client, token, account, "YESTERDAY", yesterday, yesterdayCurrency, yesterdayDate);
  if (candidate.history.status == Status::Live)
    candidate.history.status = earnings(client, token, account, "LAST_7_DAYS", last7, last7Currency, last7Date);
  if (candidate.history.status == Status::Live &&
      (yesterdayCurrency != candidate.currency || last7Currency != candidate.currency ||
       yesterdayDate != previousDay(candidate.day) || last7Date != yesterdayDate))
    candidate.history.status = Status::DateMismatch;
  if (candidate.history.status == Status::Live) {
    candidate.yesterday = yesterday; candidate.last7 = last7;
    candidate.historyCurrency = yesterdayCurrency;
    candidate.history.updated = time(nullptr);
  }
  candidate.payments.status = payments(client, token, account, candidate.balance,
    candidate.lastPayment, candidate.lastPaymentDate);
  if (candidate.payments.status == Status::Live) candidate.payments.updated = time(nullptr);
  // Optional groups retain their own last-success times and stale markers.
  Serial.printf("AdSense history: %s; payments: %s\n",
    statusLabel(candidate.history.status), statusLabel(candidate.payments.status));
  metrics = candidate;
  return Status::Live;
}

Status fetchGa4(const Connection& connection, Ga4Metrics& metrics) {
  const GoogleClient client;
  String token;
  Status status = client.authorize(connection, token);
  if (status != Status::Live) return status;
  Ga4Metrics candidate;
  String realtime[5], historical[5];
  status = gaReport(client, token, connection.resource, true, realtime);
  if (status != Status::Live) return status;
  status = gaReport(client, token, connection.resource, false, historical);
  if (status != Status::Live) return status;
  candidate.realtimeUsers = realtime[0]; candidate.newUsers28 = historical[0];
  candidate.totalUsers28 = historical[1]; candidate.sessions28 = historical[2];
  candidate.views28 = historical[3]; candidate.engagementRate28 = historical[4];
  // Publish the complete six-value snapshot or retain the previous one.
  metrics = candidate;
  return Status::Live;
}

Status fetchGsc(const Connection& connection, GscMetrics& metrics) {
  const GoogleClient client;
  String token;
  Status status = client.authorize(connection, token);
  if (status != Status::Live) return status;
  GscMetrics candidate;
  gscPeriod(candidate.start, candidate.end);
  const String body = "{\"startDate\":\"" + candidate.start + "\",\"endDate\":\"" + candidate.end
    + "\",\"type\":\"web\",\"aggregationType\":\"byProperty\",\"dataState\":\"all\"}";
  auto response = client.post(GoogleApi::GSC, "https://www.googleapis.com/webmasters/v3/sites/"
    + GoogleClient::encode(connection.resource) + "/searchAnalytics/query", body, token);
  if (response.status != Status::Live) return response.status;
  const cJSON* rows = cJSON_GetObjectItemCaseSensitive(response.document.get(), "rows");
  // With no dimensions, the existing request returns one property-wide row
  // containing all four metrics. Reject partial reports before replacing cache.
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) != 1) return Status::NoData;
  const cJSON* row = cJSON_GetArrayItem(rows, 0);
  const cJSON* clicks = cJSON_GetObjectItemCaseSensitive(row, "clicks");
  const cJSON* impressions = cJSON_GetObjectItemCaseSensitive(row, "impressions");
  const cJSON* ctr = cJSON_GetObjectItemCaseSensitive(row, "ctr");
  const cJSON* position = cJSON_GetObjectItemCaseSensitive(row, "position");
  for (const cJSON* count : {clicks, impressions}) {
    if (!cJSON_IsNumber(count) || !isfinite(count->valuedouble) || count->valuedouble < 0 ||
        count->valuedouble > 9007199254740991.0 || floor(count->valuedouble) != count->valuedouble) return Status::NoData;
  }
  if (!cJSON_IsNumber(ctr) || !isfinite(ctr->valuedouble) || ctr->valuedouble < 0 || ctr->valuedouble > 1 ||
      !cJSON_IsNumber(position) || !isfinite(position->valuedouble) || position->valuedouble < 0 ||
      (impressions->valuedouble > 0 && position->valuedouble < 1)) return Status::NoData;
  // Keep complete integer counts. CTR is a 0–1 fraction; position is already
  // an average, not a percentage. Empty reports never become fabricated zeros.
  candidate.clicks = String(clicks->valuedouble, 0);
  candidate.impressions = String(impressions->valuedouble, 0);
  candidate.ctr = String(ctr->valuedouble * 100, 1) + "%";
  candidate.position = impressions->valuedouble > 0 ? String(position->valuedouble, 1) : "--";
  metrics = candidate;
  return Status::Live;
}

Status fetchCrux(const Connection& connection, CruxMetrics& metrics) {
  Json payload(cJSON_CreateObject());
  cJSON_AddStringToObject(payload.get(), "origin", connection.resource.c_str());
  cJSON_AddStringToObject(payload.get(), "formFactor", "PHONE");
  const char* names[] = {"largest_contentful_paint", "interaction_to_next_paint", "cumulative_layout_shift"};
  cJSON_AddItemToObject(payload.get(), "metrics", cJSON_CreateStringArray(names, 3));
  char* body = cJSON_PrintUnformatted(payload.get());
  if (!body) return Status::MemoryError;
  auto response = GoogleClient{}.post(GoogleApi::CrUX,
    "https://chromeuxreport.googleapis.com/v1/records:queryRecord?key=" + GoogleClient::encode(connection.apiKey), body);
  cJSON_free(body);
  CruxMetrics candidate;
  if (response.httpCode == 404) {
    candidate.assessment = "NO DATA";
    candidate.detailsLoaded = true;
    metrics = candidate;
    return Status::Live;
  }
  if (response.status != Status::Live) return response.status;
  const cJSON* record = cJSON_GetObjectItemCaseSensitive(response.document.get(), "record");
  const cJSON* values = cJSON_GetObjectItemCaseSensitive(record, "metrics");
  const CwvThresholds limits[] = {kLcpThresholds, kInpThresholds, kClsThresholds};
  int valid = 0;
  bool failed = false;
  for (int i = 0; i < 3; ++i) {
    const cJSON* metric = cJSON_GetObjectItemCaseSensitive(values, names[i]);
    const cJSON* percentiles = cJSON_GetObjectItemCaseSensitive(metric, "percentiles");
    const cJSON* p75 = cJSON_GetObjectItemCaseSensitive(percentiles, "p75");
    double value = NAN;
    if (cJSON_IsNumber(p75)) value = p75->valuedouble;
    else if (cJSON_IsString(p75)) {
      char* end = nullptr;
      value = strtod(p75->valuestring, &end);
      if (!end || end == p75->valuestring || *end) value = NAN;
    }
    const CwvQuality quality = cwvQuality(value, limits[i]);
    if (quality != CwvQuality::NoData) {
      ++valid;
      if (quality != CwvQuality::Good) failed = true;
      if (i == 0) candidate.lcp = value;
      else if (i == 1) candidate.inp = value;
      else candidate.cls = value;
    }
  }
  // PASS requires all three p75 metrics; FAIL describes site performance,
  // independently of the request's LIVE status.
  candidate.assessment = failed ? "FAIL" : valid == 3 ? "PASS" : "NO DATA";
  const cJSON* period = cJSON_GetObjectItemCaseSensitive(record, "collectionPeriod");
  candidate.firstDate = calendarDate(cJSON_GetObjectItemCaseSensitive(period, "firstDate"), 2020);
  candidate.lastDate = calendarDate(cJSON_GetObjectItemCaseSensitive(period, "lastDate"), 2020);
  if (!candidate.firstDate.length() || !candidate.lastDate.length()) return Status::CwvDateError;
  candidate.detailsLoaded = true;
  metrics = candidate;
  return Status::Live;
}
}
