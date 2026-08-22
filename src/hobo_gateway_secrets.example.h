#pragma once

// Copy this file to src/hobo_gateway_secrets.h before building.
// hobo_gateway_secrets.h is intentionally ignored by git.

#define HOBO_HTTP_GATEWAY_ENABLED 1
#define HOBO_HTTP_GATEWAY_URL "https://meshtastic-ecru.vercel.app/api/ingest"

// Use the same secret configured as INGEST_KEY in the Vercel project.
#define HOBO_HTTP_GATEWAY_INGEST_KEY "PASTE_VERCEL_INGEST_KEY_HERE"

#define HOBO_HTTP_GATEWAY_NAME "Heltec Hub"

// Recommended: only upload packets from nodes marked Favorite on the Heltec.
// This prevents unrelated public LongFast telemetry from entering the private database.
#define HOBO_HTTP_GATEWAY_FAVORITES_ONLY 1
