#pragma once

// Copy this file to secrets.h and fill in real values.
// secrets.h is gitignored — never commit passwords or your hub hostname.

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef SUPERMINI_HUB_HOST
#define SUPERMINI_HUB_HOST "YOUR_HUB"
#endif

#ifndef SUPERMINI_API_BASE
#define SUPERMINI_API_BASE "http://YOUR_HUB/supermini"
#endif

// Shared with companion hub web login (X-SuperMini-Key)
#ifndef SUPERMINI_API_KEY
#define SUPERMINI_API_KEY "YOUR_SUPERMINI_PASSWORD"
#endif
