#include "metrics_cache.h"
#include "json_helpers.h"
#include <math.h>
#include <new>

namespace dashboard {
namespace {
constexpr size_t kMaxCacheBytes = 2048;
constexpr int kCacheVersion = 4; // V3 adds GA4 details; V4 adds GSC impressions/CTR/position.
const char* const recordKeys[] = {"adsRecord", "gaRecord", "gscRecord", "cwvRecord"};
const char* const legacySourceKeys[] = {"adsSource", "gaSource", "gscSource", "cwvSource"};
const char* const legacyTimeKeys[] = {"updated", "gaNewUpdated", "gscUpdated", "cwvUpdated"};

bool readString(const cJSON* doc, const char* key, String& value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(doc, key);
  if (!cJSON_IsString(item) || strlen(item->valuestring) > 128) return false;
  value = item->valuestring;
  return true;
}
bool readTimestamp(const cJSON* doc, const char* key, time_t& value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(doc, key);
  if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < 0 ||
      item->valuedouble > 9007199254740991.0 || floor(item->valuedouble) != item->valuedouble) return false;
  value = time_t(item->valuedouble);
  return true;
}
bool readPercentile(const cJSON* doc, const char* key, double& value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(doc, key);
  if (cJSON_IsNull(item)) { value = NAN; return true; }
  if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < 0) return false;
  value = item->valuedouble;
  return true;
}
bool readMetrics(Service service, const cJSON* doc, DashboardData& data, int version = kCacheVersion) {
  switch (service) {
    case AdSense: {
      auto& ads = data.adsense;
      if (!readString(doc, "today", ads.today) || !readString(doc, "month", ads.month) ||
          !readString(doc, "currency", ads.currency) || !readString(doc, "day", ads.day)) return false;
      if (version == 1) return true;
      return readString(doc, "yesterday", ads.yesterday) && readString(doc, "last7", ads.last7) &&
        readString(doc, "historyCurrency", ads.historyCurrency) && readString(doc, "balance", ads.balance) &&
        readString(doc, "lastPayment", ads.lastPayment) && readString(doc, "paymentDate", ads.lastPaymentDate) &&
        readTimestamp(doc, "historyUpdated", ads.history.updated) && readTimestamp(doc, "paymentsUpdated", ads.payments.updated);
    }
    case GA4: {
      auto& ga4 = data.ga4;
      if (!readString(doc, "live", ga4.realtimeUsers) || !readString(doc, "new28", ga4.newUsers28)) return false;
      if (version < 3) return true; // Retain old overview values; unknown extras stay "--".
      return readString(doc, "total28", ga4.totalUsers28) && readString(doc, "sessions28", ga4.sessions28) &&
        readString(doc, "views28", ga4.views28) && readString(doc, "engagement28", ga4.engagementRate28);
    }
    case GSC: {
      auto& gsc = data.gsc;
      if (!readString(doc, "clicks", gsc.clicks) || !readString(doc, "start", gsc.start) ||
          !readString(doc, "end", gsc.end)) return false;
      if (version < 4) return true; // Existing clicks/dates survive; missing details remain "--".
      return readString(doc, "impressions", gsc.impressions) && readString(doc, "ctr", gsc.ctr) &&
        readString(doc, "position", gsc.position);
    }
    case CrUX: {
      auto& cwv = data.crux;
      if (!readString(doc, "assessment", cwv.assessment) || !readString(doc, "first", cwv.firstDate) ||
          !readString(doc, "last", cwv.lastDate)) return false;
      if (version == 1) return true;
      const cJSON* loaded = cJSON_GetObjectItemCaseSensitive(doc, "detailsLoaded");
      if (!cJSON_IsBool(loaded)) return false;
      cwv.detailsLoaded = cJSON_IsTrue(loaded);
      return readPercentile(doc, "lcp", cwv.lcp) && readPercentile(doc, "inp", cwv.inp) && readPercentile(doc, "cls", cwv.cls);
    }
    default: return false;
  }
}
void writeMetrics(Service service, cJSON* doc, const DashboardData& data) {
  switch (service) {
    case AdSense:
      cJSON_AddStringToObject(doc, "today", data.adsense.today.c_str());
      cJSON_AddStringToObject(doc, "month", data.adsense.month.c_str());
      cJSON_AddStringToObject(doc, "currency", data.adsense.currency.c_str());
      cJSON_AddStringToObject(doc, "day", data.adsense.day.c_str());
      cJSON_AddStringToObject(doc, "yesterday", data.adsense.yesterday.c_str());
      cJSON_AddStringToObject(doc, "last7", data.adsense.last7.c_str());
      cJSON_AddStringToObject(doc, "historyCurrency", data.adsense.historyCurrency.c_str());
      cJSON_AddStringToObject(doc, "balance", data.adsense.balance.c_str());
      cJSON_AddStringToObject(doc, "lastPayment", data.adsense.lastPayment.c_str());
      cJSON_AddStringToObject(doc, "paymentDate", data.adsense.lastPaymentDate.c_str());
      cJSON_AddNumberToObject(doc, "historyUpdated", double(data.adsense.history.updated));
      cJSON_AddNumberToObject(doc, "paymentsUpdated", double(data.adsense.payments.updated));
      break;
    case GA4:
      cJSON_AddStringToObject(doc, "live", data.ga4.realtimeUsers.c_str());
      cJSON_AddStringToObject(doc, "new28", data.ga4.newUsers28.c_str());
      cJSON_AddStringToObject(doc, "total28", data.ga4.totalUsers28.c_str());
      cJSON_AddStringToObject(doc, "sessions28", data.ga4.sessions28.c_str());
      cJSON_AddStringToObject(doc, "views28", data.ga4.views28.c_str());
      cJSON_AddStringToObject(doc, "engagement28", data.ga4.engagementRate28.c_str());
      break;
    case GSC:
      cJSON_AddStringToObject(doc, "clicks", data.gsc.clicks.c_str());
      cJSON_AddStringToObject(doc, "start", data.gsc.start.c_str());
      cJSON_AddStringToObject(doc, "end", data.gsc.end.c_str());
      cJSON_AddStringToObject(doc, "impressions", data.gsc.impressions.c_str());
      cJSON_AddStringToObject(doc, "ctr", data.gsc.ctr.c_str());
      cJSON_AddStringToObject(doc, "position", data.gsc.position.c_str());
      break;
    case CrUX:
      cJSON_AddStringToObject(doc, "assessment", data.crux.assessment.c_str());
      cJSON_AddStringToObject(doc, "first", data.crux.firstDate.c_str());
      cJSON_AddStringToObject(doc, "last", data.crux.lastDate.c_str());
      cJSON_AddBoolToObject(doc, "detailsLoaded", data.crux.detailsLoaded);
      if (isfinite(data.crux.lcp)) cJSON_AddNumberToObject(doc, "lcp", data.crux.lcp); else cJSON_AddNullToObject(doc, "lcp");
      if (isfinite(data.crux.inp)) cJSON_AddNumberToObject(doc, "inp", data.crux.inp); else cJSON_AddNullToObject(doc, "inp");
      if (isfinite(data.crux.cls)) cJSON_AddNumberToObject(doc, "cls", data.crux.cls); else cJSON_AddNullToObject(doc, "cls");
      break;
    default: break;
  }
}
}

bool MetricsCache::begin() {
  available_ = store_.begin("ads-dashboard", false);
  return available_;
}

bool MetricsCache::save(Service service, const Connection& connection, const DashboardData& data) {
  if (!available_ || service >= ServiceCount || data.providers[service].updated <= 0) return false;
  const String source = connectionIdentity(connection);
  if (!source.length()) return false;
  Json doc(cJSON_CreateObject());
  cJSON_AddNumberToObject(doc.get(), "version", kCacheVersion);
  cJSON_AddStringToObject(doc.get(), "source", source.c_str());
  cJSON_AddNumberToObject(doc.get(), "updated", double(data.providers[service].updated));
  writeMetrics(service, doc.get(), data);
  // Reject partial JSON after any allocation failure before overwriting a good
  // cache. Values were already validated by the provider before reaching here.
  DashboardData candidate;
  const cJSON* version = cJSON_GetObjectItemCaseSensitive(doc.get(), "version");
  const cJSON* updated = cJSON_GetObjectItemCaseSensitive(doc.get(), "updated");
  if (!cJSON_IsNumber(version) || version->valuedouble != kCacheVersion ||
      !cJSON_IsNumber(updated) || updated->valuedouble != double(data.providers[service].updated) ||
      jsonText(doc.get(), "source") != source || !readMetrics(service, doc.get(), candidate)) return false;
  char* json = cJSON_PrintUnformatted(doc.get());
  if (!json) return false;
  const size_t length = strlen(json);
  const bool saved = length && length <= kMaxCacheBytes &&
    store_.putBytes(recordKeys[service], json, length) == length;
  cJSON_free(json);
  return saved;
}

bool MetricsCache::loadRecord(Service service, const String& source, DashboardData& data) {
  const size_t length = store_.getBytesLength(recordKeys[service]);
  if (!length || length > kMaxCacheBytes) return false;
  std::unique_ptr<char[]> bytes(new (std::nothrow) char[length + 1]);
  if (!bytes || store_.getBytes(recordKeys[service], bytes.get(), length) != length) return false;
  bytes[length] = 0;
  const char* end = nullptr;
  Json doc(cJSON_ParseWithLengthOpts(bytes.get(), length + 1, &end, true));
  const cJSON* version = cJSON_GetObjectItemCaseSensitive(doc.get(), "version");
  const cJSON* updated = cJSON_GetObjectItemCaseSensitive(doc.get(), "updated");
  if (!cJSON_IsObject(doc.get()) || end != bytes.get() + length ||
      !cJSON_IsNumber(version) || version->valuedouble < 1 || version->valuedouble > kCacheVersion ||
      floor(version->valuedouble) != version->valuedouble ||
      jsonText(doc.get(), "source") != source ||
      !cJSON_IsNumber(updated) || !isfinite(updated->valuedouble) || updated->valuedouble <= 0 ||
      updated->valuedouble > 9007199254740991.0 || floor(updated->valuedouble) != updated->valuedouble) return false;
  DashboardData candidate = data;
  // Older records retain overview values; missing detail fields remain unknown.
  if (!readMetrics(service, doc.get(), candidate, version->valueint)) return false;
  candidate.providers[service].updated = time_t(updated->valuedouble);
  data = candidate;
  return true;
}

bool MetricsCache::loadLegacy(Service service, const String& source, DashboardData& data) {
  // Preserve the old cache only when it was already bound to this exact Google
  // connection. Never import values from an unbound or different account.
  if (store_.getString(legacySourceKeys[service], "") != source) return false;
  const int64_t updated = store_.getLong64(legacyTimeKeys[service], 0);
  if (updated <= 0) return false;
  DashboardData candidate = data;
  switch (service) {
    case AdSense:
      if (!store_.isKey("today") || !store_.isKey("month") || !store_.isKey("currency") || !store_.isKey("day")) return false;
      candidate.adsense = {store_.getString("today"), store_.getString("month"), store_.getString("currency"), store_.getString("day")};
      break;
    case GA4:
      if (!store_.isKey("gaLive") || !store_.isKey("gaNew28")) return false;
      candidate.ga4 = {store_.getString("gaLive"), store_.getString("gaNew28")};
      break;
    case GSC:
      if (!store_.isKey("gscClicks") || !store_.isKey("gscStart") || !store_.isKey("gscEnd")) return false;
      candidate.gsc = {store_.getString("gscClicks"), store_.getString("gscStart"), store_.getString("gscEnd")};
      break;
    case CrUX:
      if (!store_.isKey("cwvValue") || !store_.isKey("cwvFirst") || !store_.isKey("cwvLast")) return false;
      candidate.crux = {store_.getString("cwvValue"), store_.getString("cwvFirst"), store_.getString("cwvLast")};
      break;
    default: return false;
  }
  candidate.providers[service].updated = time_t(updated);
  data = candidate;
  return true;
}

void MetricsCache::load(const DeviceConfig& config, DashboardData& data) {
  if (!available_) return;
  for (int i = 0; i < ServiceCount; ++i) {
    const Service service = Service(i);
    if (!config.services[i].ready(service)) continue;
    const String source = connectionIdentity(config.services[i]);
    if (!source.length()) continue;
    if (store_.isKey(recordKeys[i])) loadRecord(service, source, data);
    else if (loadLegacy(service, source, data) && !save(service, config.services[i], data))
      Serial.printf("CACHE: %s migration not persisted; using values in RAM\n", serviceNames[i]);
  }
}
}
