#pragma once

#include <Arduino.h>
#include <stdio.h>

inline void weatherSummaryLong(int code, char *out, size_t outLen) {
  const char *s = "Unknown";
  if (code == 0) s = "Clear";
  else if (code == 1) s = "Mainly clear";
  else if (code == 2) s = "Partly cloudy";
  else if (code == 3) s = "Overcast";
  else if (code == 45 || code == 48) s = "Fog";
  else if (code >= 51 && code <= 57) s = "Drizzle";
  else if (code >= 61 && code <= 67) s = "Rain";
  else if (code >= 71 && code <= 77) s = "Snow";
  else if (code >= 80 && code <= 82) s = "Showers";
  else if (code >= 95 && code <= 99) s = "Thunder";
  snprintf(out, outLen, "%s", s);
}

// Short labels for 160px rows
inline void weatherSummaryShort(int code, char *out, size_t outLen) {
  const char *s = "?";
  if (code == 0 || code == 1) s = "Clear";
  else if (code == 2) s = "P.Cld";
  else if (code == 3) s = "Overc";
  else if (code == 45 || code == 48) s = "Fog";
  else if (code >= 51 && code <= 57) s = "Drzl";
  else if (code >= 61 && code <= 67) s = "Rain";
  else if (code >= 71 && code <= 77) s = "Snow";
  else if (code >= 80 && code <= 82) s = "Shwr";
  else if (code >= 95 && code <= 99) s = "Thun";
  snprintf(out, outLen, "%s", s);
}
