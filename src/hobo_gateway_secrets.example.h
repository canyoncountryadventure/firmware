#pragma once

// Copy this example to src/modules/hobo_gateway_secrets.h before building.
// The real local header is intentionally ignored by git.

#define HOBO_HTTP_GATEWAY_ENABLED 1
#define HOBO_HTTP_GATEWAY_URL "https://meshtastic-ecru.vercel.app/api/ingest"

// Use the same secret configured as INGEST_KEY in the Vercel project.
#define HOBO_HTTP_GATEWAY_INGEST_KEY "PASTE_VERCEL_INGEST_KEY_HERE"

#define HOBO_HTTP_GATEWAY_NAME "Heltec Hub (Home)"

// Node-ID allowlist + strict MX2001/SM-soil/DS-water/telemetry packet decoding.
// Canonical station names are assigned by the cloud, not embedded here.
#define HOBO_HTTP_GATEWAY_FAVORITES_ONLY 0
