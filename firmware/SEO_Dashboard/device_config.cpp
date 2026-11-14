#include "device_config.h"
#include <math.h>
#include <mbedtls/md.h>

namespace dashboard {
const char* const serviceNames[ServiceCount] = {"adsense", "ga4", "gsc", "crux"};
namespace {
bool stringField(cJSON* object, const char* name, String& value, size_t maximum) {
  cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
  if (!cJSON_IsString(item) || strlen(item->valuestring) > maximum) return false;
  value = item->valuestring;
  return true;
}
bool printable(const String& value, size_t maximum) {
  if (value.length() > maximum || value.startsWith("YOUR_")) return false;
  for (size_t i=0; i<value.length(); ++i)
    if ((uint8_t)value[i] <= 32 || (uint8_t)value[i] >= 127) return false;
  return true;
}
bool originValid(const String& value) {
  int start = value.startsWith("https://") ? 8 : value.startsWith("http://") ? 7 : 0;
  if (!start || value.length() <= size_t(start)) return false;
  String host = value.substring(start);
  if (host.indexOf('/') >= 0 || host.indexOf('?') >= 0 || host.indexOf('#') >= 0 || host.indexOf('@') >= 0) return false;
  for (size_t i=0; i<host.length(); ++i)
    if (!isalnum((unsigned char)host[i]) && host[i]!='.' && host[i]!='-' && host[i]!=':') return false;
  return true;
}
cJSON* networksJson(const DeviceConfig& config, bool secrets) {
  cJSON* result = cJSON_CreateArray();
  for (size_t i = 0; i < networkCount(config); ++i) {
    const NetworkProfile& network = networkProfile(config, i);
    cJSON* item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", network.ssid);
    if (secrets) cJSON_AddStringToObject(item, "password", network.password);
    else cJSON_AddBoolToObject(item, "passwordSet", network.password[0] != 0);
    cJSON_AddItemToArray(result, item);
  }
  return result;
}
cJSON* connectionsJson(const DeviceConfig& config, bool secrets) {
  // Only the private NVS serializer enables secrets. The browser needs presence
  // flags to leave password inputs empty while preserving existing values.
  cJSON* result = cJSON_CreateObject();
  for (int i=0; i<ServiceCount; ++i) {
    const Connection& connection = config.services[i];
    cJSON* item = cJSON_AddObjectToObject(result, serviceNames[i]);
    cJSON_AddBoolToObject(item, "enabled", connection.enabled);
    cJSON_AddStringToObject(item, "resource", connection.resource.c_str());
    if (i != CrUX) {
      cJSON_AddStringToObject(item, "clientId", connection.clientId.c_str());
      if (secrets) {
        cJSON_AddStringToObject(item, "clientSecret", connection.clientSecret.c_str());
        cJSON_AddStringToObject(item, "refreshToken", connection.refreshToken.c_str());
      } else {
        cJSON_AddBoolToObject(item, "clientSecretSet", connection.clientSecret.length() != 0);
        cJSON_AddBoolToObject(item, "refreshTokenSet", connection.refreshToken.length() != 0);
      }
    } else if (secrets) cJSON_AddStringToObject(item, "apiKey", connection.apiKey.c_str());
    else cJSON_AddBoolToObject(item, "apiKeySet", connection.apiKey.length() != 0);
  }
  return result;
}
}
const NetworkProfile& networkProfile(const DeviceConfig& config, size_t index) {
  return index == 0 ? static_cast<const NetworkProfile&>(config.network) : config.fallbackNetworks[index - 1];
}
NetworkProfile& networkProfile(DeviceConfig& config, size_t index) {
  return index == 0 ? static_cast<NetworkProfile&>(config.network) : config.fallbackNetworks[index - 1];
}
size_t networkCount(const DeviceConfig& config) {
  size_t count = 0;
  while (count < kMaxWifiNetworks && networkProfile(config, count).ssid[0]) ++count;
  return count;
}
bool sameNetworks(const DeviceConfig& first, const DeviceConfig& second) {
  for (size_t i = 0; i < kMaxWifiNetworks; ++i) {
    const auto& a = networkProfile(first, i);
    const auto& b = networkProfile(second, i);
    if (strcmp(a.ssid, b.ssid) || strcmp(a.password, b.password)) return false;
  }
  return true;
}
const char* validateNetworks(const DeviceConfig& config) {
  if (const char* error = validate(config.network)) return error;
  bool empty = false;
  for (size_t i = 0; i < kMaxWifiNetworks; ++i) {
    const auto& network = networkProfile(config, i);
    if (!network.ssid[0]) {
      if (network.password[0]) return "An unused Wi-Fi slot cannot contain a password.";
      empty = true;
      continue;
    }
    if (empty) return "Remove empty Wi-Fi slots between networks.";
    if (const char* error = validateNetwork(network)) return error;
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(network.ssid, networkProfile(config, j).ssid)) return "Each Wi-Fi network name must be unique.";
  }
  return nullptr;
}
const char* networksFromForm(cJSON* json, const DeviceConfig& current, DeviceConfig& candidate) {
  if (!cJSON_IsArray(json) || cJSON_GetArraySize(json) < 1 || cJSON_GetArraySize(json) > int(kMaxWifiNetworks))
    return "Add between one and three Wi-Fi networks.";
  for (size_t i = 0; i < kMaxWifiNetworks; ++i) networkProfile(candidate, i) = NetworkProfile{};
  for (int i = 0; i < cJSON_GetArraySize(json); ++i) {
    cJSON* item = cJSON_GetArrayItem(json, i);
    cJSON* open = cJSON_GetObjectItemCaseSensitive(item, "openNetwork");
    String ssid, password;
    if (!cJSON_IsObject(item) || !cJSON_IsBool(open) ||
        !stringField(item, "ssid", ssid, 32) || !stringField(item, "password", password, 64))
      return "Check each Wi-Fi network name, password and Open network option.";
    NetworkProfile saved;
    // Password retention follows the SSID, not its row or priority. Reordering
    // networks must never attach one network's password to another network.
    for (size_t j = 0; j < networkCount(current); ++j)
      if (ssid == networkProfile(current, j).ssid) { saved = networkProfile(current, j); break; }
    if (const char* error = networkFromForm(saved, ssid.c_str(), password.c_str(),
                                           cJSON_IsTrue(open), networkProfile(candidate, size_t(i)))) return error;
  }
  return validateNetworks(candidate);
}
bool Connection::ready(Service service) const {
  return enabled && (service == CrUX ? apiKey.length() && resource.length() :
    clientId.length() && clientSecret.length() && refreshToken.length() && (service == AdSense || resource.length()));
}
const char* validateTitle(const String& title) {
  if (!title.length() || title.length() > kMaxTitleLength)
    return "Display title must contain 1-20 characters.";
  bool visible = false;
  for (size_t i = 0; i < title.length(); ++i) {
    const uint8_t character = uint8_t(title[i]);
    if (character < 32 || character > 126)
      return "Display title supports Latin letters, digits, punctuation, and spaces only.";
    if (character != ' ') visible = true;
  }
  return visible ? nullptr : "Display title cannot contain only spaces.";
}
const char* validateConnections(const DeviceConfig& config) {
  for (int i=0; i<ServiceCount; ++i) {
    const Connection& c = config.services[i];
    if (!printable(c.clientId,512) || !printable(c.clientSecret,512) ||
        !printable(c.refreshToken,2048) || !printable(c.apiKey,512) || !printable(c.resource,512))
      return "Connection fields must be ASCII without spaces; check their lengths and remove placeholders.";
    if (c.enabled && !c.ready(Service(i))) return "Complete the credentials and resource for each enabled service, or turn it off.";
    if (i==GA4 && c.resource.length())
      for (size_t j=0; j<c.resource.length(); ++j)
        if (c.resource.length()>20 || !isDigit(c.resource[j])) return "GA4 Property ID must contain 1-20 digits.";
    if (i==GSC && c.resource.length()) {
      bool valid = c.resource.startsWith("sc-domain:") && c.resource.length()>10;
      if (valid) for (size_t j=10; j<c.resource.length(); ++j)
        valid = valid && (isalnum((unsigned char)c.resource[j]) || c.resource[j]=='.' || c.resource[j]=='-');
      if (!valid && !c.resource.startsWith("https://") && !c.resource.startsWith("http://"))
        return "Use a Search Console resource such as sc-domain:example.com or https://example.com/.";
    }
    if (i==CrUX && c.resource.length() && !originValid(c.resource))
      return "CrUX origin must be http(s)://hostname, without a path, query or trailing slash.";
  }
  return nullptr;
}
cJSON* publicConfig(const DeviceConfig& config) {
  cJSON* result = cJSON_CreateObject();
  cJSON_AddStringToObject(result,"title",config.title.c_str());
  cJSON_AddItemToObject(result,"networks",networksJson(config,false));
  cJSON_AddBoolToObject(result,"dark",config.network.dark);
  cJSON_AddBoolToObject(result,"rotate180",config.network.rotate180);
  cJSON_AddStringToObject(result,"timeZone",config.network.timeZone);
  cJSON_AddNumberToObject(result,"intervalMinutes",config.network.intervalMinutes);
  cJSON_AddItemToObject(result,"services",connectionsJson(config,false));
  return result;
}
String encodeConfig(const DeviceConfig& config) {
  cJSON* doc = publicConfig(config);
  cJSON_AddNumberToObject(doc,"version",4);
  cJSON_ReplaceItemInObjectCaseSensitive(doc,"networks",networksJson(config,true));
  cJSON_ReplaceItemInObjectCaseSensitive(doc,"services",connectionsJson(config,true));
  char* bytes = cJSON_PrintUnformatted(doc);
  String result = bytes ? bytes : "";
  cJSON_free(bytes); cJSON_Delete(doc);
  return result;
}
const char* connectionsFromForm(cJSON* json, const DeviceConfig& current, DeviceConfig& candidate) {
  if (!cJSON_IsObject(json)) return "Missing Google connections.";
  for (int i=0; i<ServiceCount; ++i) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(json,serviceNames[i]);
    cJSON* enabled = cJSON_GetObjectItemCaseSensitive(item,"enabled");
    cJSON* clear = cJSON_GetObjectItemCaseSensitive(item,"clearCredentials");
    if (!cJSON_IsObject(item) || !cJSON_IsBool(enabled) || (clear && !cJSON_IsBool(clear))) return "Invalid Google connection.";
    Connection c = current.services[i];
    // Clearing is staged with the rest of the form, not a separate NVS write.
    if (cJSON_IsTrue(clear)) { c.clientId=""; c.clientSecret=""; c.refreshToken=""; c.apiKey=""; }
    c.enabled = cJSON_IsTrue(enabled);
    if (!stringField(item,"resource",c.resource,512)) return "Invalid resource.";
    if (i==CrUX && c.resource.endsWith("/")) c.resource.remove(c.resource.length()-1);
    String secret, refresh;
    if (i==CrUX) {
      if (!stringField(item,"apiKey",secret,512)) return "Invalid CrUX key.";
      if (secret.length()) c.apiKey=secret;
    } else {
      if (!stringField(item,"clientId",c.clientId,512) || !stringField(item,"clientSecret",secret,512) ||
          !stringField(item,"refreshToken",refresh,2048)) return "Invalid OAuth fields.";
      // Tokens and secrets belong to one OAuth client. Blank fields may retain
      // saved values only while that client stays the same.
      if (c.clientId != current.services[i].clientId) {
        c.clientSecret = "";
        c.refreshToken = "";
        if (c.enabled && (!secret.length() || !refresh.length()))
          return "When changing Client ID, enter its client secret and a matching refresh token.";
      }
      if (secret.length()) c.clientSecret=secret;
      if (refresh.length()) c.refreshToken=refresh;
    }
    candidate.services[i]=c;
  }
  return validateConnections(candidate);
}
bool decodeConfig(const String& json, DeviceConfig& config) {
  if (!json.length() || json.length()>kMaxConfigBytes || json.indexOf("\\u0000")>=0) return false;
  const char* end=nullptr;
  cJSON* doc=cJSON_ParseWithLengthOpts(json.c_str(),json.length()+1,&end,true);
  auto* version=cJSON_GetObjectItemCaseSensitive(doc,"version");
  auto* dark=cJSON_GetObjectItemCaseSensitive(doc,"dark");
  auto* interval=cJSON_GetObjectItemCaseSensitive(doc,"intervalMinutes");
  auto* title=cJSON_GetObjectItemCaseSensitive(doc,"title");
  DeviceConfig candidate;
  // V1's binary record is migrated by settings_store. V2/V3 JSON has one
  // network; preserve its credentials as priority 1 and use the old display defaults.
  bool ok=cJSON_IsObject(doc) && end==json.c_str()+json.length() && cJSON_IsNumber(version) &&
    (version->valuedouble==2 || version->valuedouble==3 || version->valuedouble==4) &&
    cJSON_IsBool(dark) && cJSON_IsNumber(interval) && isfinite(interval->valuedouble) &&
    floor(interval->valuedouble)==interval->valuedouble && interval->valuedouble>=5 && interval->valuedouble<=1440;
  if (ok && (title || version->valuedouble>=3))
    ok=stringField(doc,"title",candidate.title,kMaxTitleLength);
  if (ok) {
    candidate.network.dark=cJSON_IsTrue(dark);
    candidate.network.intervalMinutes=uint16_t(interval->valuedouble);
    if (version->valuedouble==4) {
      cJSON* networks=cJSON_GetObjectItemCaseSensitive(doc,"networks");
      cJSON* rotated=cJSON_GetObjectItemCaseSensitive(doc,"rotate180");
      String zone;
      ok=cJSON_IsBool(rotated) && stringField(doc,"timeZone",zone,sizeof(candidate.network.timeZone)-1) &&
        cJSON_IsArray(networks) && cJSON_GetArraySize(networks)>=1 && cJSON_GetArraySize(networks)<=int(kMaxWifiNetworks);
      if (ok) {
        candidate.network.rotate180=cJSON_IsTrue(rotated);
        strlcpy(candidate.network.timeZone,zone.c_str(),sizeof(candidate.network.timeZone));
        for (int i=0;ok && i<cJSON_GetArraySize(networks);++i) {
          String ssid,password;
          cJSON* item=cJSON_GetArrayItem(networks,i);
          ok=cJSON_IsObject(item) && stringField(item,"ssid",ssid,32) && stringField(item,"password",password,64);
          if (ok) {
            NetworkProfile& network=networkProfile(candidate,size_t(i));
            strlcpy(network.ssid,ssid.c_str(),sizeof(network.ssid));
            strlcpy(network.password,password.c_str(),sizeof(network.password));
          }
        }
      }
    } else {
      String ssid,password;
      ok=stringField(doc,"ssid",ssid,32) && stringField(doc,"password",password,64);
      if (ok) {
        strlcpy(candidate.network.ssid,ssid.c_str(),sizeof(candidate.network.ssid));
        strlcpy(candidate.network.password,password.c_str(),sizeof(candidate.network.password));
      }
    }
    if (ok) ok=!validateNetworks(candidate) && !validateTitle(candidate.title) &&
      !connectionsFromForm(cJSON_GetObjectItemCaseSensitive(doc,"services"),DeviceConfig{},candidate);
  }
  cJSON_Delete(doc);
  if (ok) config=candidate;
  return ok;
}

String connectionIdentity(const Connection& connection) {
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  const mbedtls_md_info_t* info=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info || mbedtls_md_setup(&context,info,0)!=0) { mbedtls_md_free(&context); return ""; }
  int result=mbedtls_md_starts(&context);
  const String* fields[]={&connection.clientId,&connection.clientSecret,&connection.refreshToken,&connection.resource,&connection.apiKey};
  // Include each terminator to distinguish field boundaries. Title, theme, Wi-Fi, and
  // the enabled flag are excluded so those changes preserve provider caches.
  for (const String* field:fields)
    result |= mbedtls_md_update(&context,reinterpret_cast<const unsigned char*>(field->c_str()),field->length()+1);
  unsigned char digest[32]; result |= mbedtls_md_finish(&context,digest);
  mbedtls_md_free(&context);
  if (result) return "";
  char encoded[65]; const char hex[]="0123456789abcdef";
  for (int i=0;i<32;++i) { encoded[i*2]=hex[digest[i]>>4]; encoded[i*2+1]=hex[digest[i]&15]; }
  encoded[64]=0; return encoded;
}
}
