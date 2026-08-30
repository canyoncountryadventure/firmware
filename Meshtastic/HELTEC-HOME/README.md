# Heltec Home Sensor Gateway

**Branch:** `cca-heltec-sensor-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`

Heltec Home is the internet-connected aggregation point for the CCA Meshtastic sensor network. It remains a normal Meshtastic radio/server while running the local HOBO and remote sensor gateway functions.

## HOBO station acquisition modes

| Station | Acquisition mode |
|---|---|
| **Home** | **Automatic local BLE reading on the Heltec** |
| **Hidden Valley** | **Automatic remote Meshtastic telemetry** |
| **Fishlake Hightop** | **Heltec-triggered remote `READ`** |

The key rule is:

> Fishlake is triggered by Heltec to read. Hidden Valley and Home are automatic.

## Home

The Heltec directly connects to its selected Home HOBO over BLE. Automatic local measurements enter the Home gateway path without requiring a Meshtastic DM from another node.

Home temperature readings are held locally until a Hidden Valley environmental packet arrives, allowing Home and Hidden Valley to share one HTTPS batch request when possible.

## Hidden Valley

Hidden Valley is expected to transmit standard Meshtastic environmental telemetry automatically from its remote RAK/HOBO node. The Heltec receives those packets normally.

A Hidden Valley environmental temperature packet is also the cloud flush trigger for any pending Home temperature readings.

## Fishlake Hightop

Fishlake is deliberately different. `FishlakePollerModule` on the Heltec sends the remote node a direct Meshtastic text command:

```text
READ
```

Current Fishlake node:

```text
!5e021e35
```

The remote node performs a fresh HOBO read and sends a text reply. The Heltec parses that reply and uploads the result as station `Fishlake Hightop`.

Current trigger interval:

```text
60 minutes
```

Fishlake therefore does not need to automatically broadcast HOBO temperature on its own schedule.

## Cloud behavior

Home + Hidden Valley:

```text
Home automatic BLE reading
        |
        v
held locally on Heltec
        |
        | Hidden Valley automatic telemetry arrives
        v
single HTTPS batch
[Hidden Valley, Home pending reading(s)]
        |
        v
Vercel -> Neon
```

Fishlake:

```text
Heltec --READ--> Fishlake
Heltec <--reply-- Fishlake
  |
  +--> HTTPS --> Vercel --> Neon
```

## Normal Meshtastic operation

Sensor gateway features must not replace the primary radio functions. The Heltec continues to provide normal Meshtastic LoRa operation plus its TCP/API, web, Wi-Fi, and OTA services.

## Build

Use the GitHub Actions workflow:

```text
Build CCA Heltec Sensor Gateway
```

Target:

```text
heltec-v4 / esp32s3
```

## Wi-Fi OTA

Routine updates use Meshtastic Unified Wi-Fi OTA with the regular Heltec V4 application `.bin`. Preserve NVS/configuration and the existing OTA loader.

Ports:

- Meshtastic TCP API: `4403`
- Unified OTA loader: `3232`
