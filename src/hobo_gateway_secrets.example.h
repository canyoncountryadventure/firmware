#pragma once

// Copy this example to src/modules/hobo_gateway_secrets.h before building.
// The real local header is intentionally ignored by git.

#define HOBO_HTTP_GATEWAY_ENABLED 1
#define HOBO_HTTP_GATEWAY_URL "https://meshtastic-ecru.vercel.app/api/ingest"

// Use the same secret configured as INGEST_KEY in the Vercel project.
#define HOBO_HTTP_GATEWAY_INGEST_KEY "PASTE_VERCEL_INGEST_KEY_HERE"

#define HOBO_HTTP_GATEWAY_NAME "Heltec Hub (Home)"

// MX2001-only build: accept any valid 19-byte PRIVATE_APP packet beginning with "MX".
// Normal Meshtastic environmental telemetry is ignored by the gateway.
#define HOBO_HTTP_GATEWAY_FAVORITES_ONLY 0
