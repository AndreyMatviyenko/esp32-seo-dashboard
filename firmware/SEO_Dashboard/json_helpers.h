#pragma once

#include <Arduino.h>
#include <memory>
#include "cJSON.h"

namespace dashboard {
struct JsonDeleter { void operator()(cJSON* value) const { cJSON_Delete(value); } };
using Json = std::unique_ptr<cJSON, JsonDeleter>;

inline String jsonText(const cJSON* object, const char* key) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(item) ? String(item->valuestring) : String();
}
}
