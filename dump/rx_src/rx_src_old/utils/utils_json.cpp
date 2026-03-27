#include "utils_json.h"

String extractValue(String payload, String key) {
  int start = payload.indexOf(key);
  if (start == -1) return "";

  start = payload.indexOf(":", start) + 2;
  int end = payload.indexOf("\"", start);

  return payload.substring(start, end);
}