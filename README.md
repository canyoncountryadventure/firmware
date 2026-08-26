# CCA TEST BRANCH — MX + HOBO + PIR

> **YOU ARE VIEWING:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **For the Seeed XIAO nRF52840 + Wio-SX1262 + HOBO MX + SEN0171 PIR build, open:**
> **[`START_HERE_CCA_MX_PIR.md`](START_HERE_CCA_MX_PIR.md)**
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`  
> **Firmware identity:** `CCA-MX-PIR 1.0.0` / Schema 1  
> **PIR SIGNAL PIN:** **D6 — NOT D0**  
> **D0/A0:** reserved for future soil-moisture sensor; soil is **not included in v1**
>
> Do not follow the normal production build target below when testing the CCA PIR firmware. The detailed CCA guide above is authoritative for this branch.

---

# Meshtastic HOBO Firmware

Custom Meshtastic firmware for remotely reading Onset HOBO loggers over Bluetooth and transmitting their measurements over the Meshtastic mesh.

## Production status

**Production branch:** `hobo-mx2001-mx2201-mx2203`  
**Frozen validated snapshot:** `hobo-universal-validated-2026-08-19`

The same universal firmware supports both:

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 / RAK19003

and all three supported HOBO logger families:

| HOBO | Automatic data | Direct `READ` |
|---|---|---|
| MX2001 | Water level + temperature | Water level + temperature |
| MX2201 | Temperature | Temperature |
| MX2203 | Temperature | Temperature |

## Automatic telemetry is tied to the HOBO logging interval

Automatic packets are **not** produced by an independent free-running radio timer.

The radio reads the HOBO `STATUS` response, learns the logger's configured interval and current write pointer, then waits for the write pointer to advance. Each confirmed new HOBO record triggers:

1. one fresh `NEWREAD64` read;
2. one Meshtastic telemetry packet;
3. write-pointer advancement only after the packet is successfully queued.

If `STATUS` tracking fails, automatic telemetry pauses instead of guessing the schedule.

Final RAK4631 hardware validation on MX2201 at a 20-second logger interval produced consecutive automatic packet cadences of **19.848 s** and **19.879 s**, with the packet queued about **202 ms** after the new logger record was detected.

## Meshtastic commands

Send these as direct text messages to the field radio:

- `LOGGER` — show connected HOBO model, MAC, BLE RSSI, logging interval, and lock state.
- `READ` — perform an immediate fresh read without disturbing the automatic schedule.
- `LOCK` — save the currently identified HOBO BLE MAC to flash and reconnect only to that logger after reboot.
- `UNLOCK` — clear the saved assignment and resume discovery of any supported HOBO.

A leading slash is optional and command matching is case-insensitive.

For field deployment, leave radios unlocked during bench work. At the monitoring site, verify the intended logger with `LOGGER`, then use `LOCK`.

## Start here

**Open:** [`Meshtastic/README.md`](Meshtastic/README.md)

```text
Meshtastic/
├── SEEED-XIAO/     ← Seeed XIAO nRF52840 + Wio-SX1262
├── RAK4631/        ← RAK4631 / RAK19003
├── SHARED-HOBO/    ← automatic telemetry, commands, shared BLE protocol
└── ARCHIVE/        ← old branches and recovery history
```

## Radio guides

- Seeed: [`Meshtastic/SEEED-XIAO/README.md`](Meshtastic/SEEED-XIAO/README.md)
- RAK4631: [`Meshtastic/RAK4631/README.md`](Meshtastic/RAK4631/README.md)
- Shared HOBO behavior: [`Meshtastic/SHARED-HOBO/README.md`](Meshtastic/SHARED-HOBO/README.md)

## PlatformIO targets

Seeed:

```text
seeed_xiao_nrf52840_kit
```

RAK4631:

```text
rak4631
```

## Local Windows repository

Current working location:

```text
C:\Meshtastic-HOBO\firmware
```

Sync the production branch with:

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

The rest of this repository remains the full Meshtastic source tree because `src/`, `variants/`, `lib/`, PlatformIO configuration, and related directories are required to compile the firmware.
