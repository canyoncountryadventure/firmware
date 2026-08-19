# Meshtastic HOBO Firmware

Custom Meshtastic firmware for remotely reading Onset HOBO loggers over Bluetooth and returning the measurements over the Meshtastic mesh.

## Start here

**Open:** [`Meshtastic/README.md`](Meshtastic/README.md)

That folder is the organized, user-facing project structure:

```text
Meshtastic/
├── SEEED-XIAO/     ← Seeed XIAO nRF52840 + Wio-SX1262
├── RAK4631/        ← RAK4631 / RAK19003
├── SHARED-HOBO/    ← common MX2001/MX2201/MX2203 protocol
└── ARCHIVE/        ← old branches and recovery history
```

## Production branch

Use only this branch for normal deployments:

```text
hobo-mx2001-mx2201-mx2203
```

It is hardware validated on both supported radios and supports:

| HOBO | Data returned by `READ` |
|---|---|
| MX2001 | Water level + temperature |
| MX2201 | Temperature |
| MX2203 | Temperature |

## Radio guides

### Seeed

[`Meshtastic/SEEED-XIAO/README.md`](Meshtastic/SEEED-XIAO/README.md)

PlatformIO target:

```text
seeed_xiao_nrf52840_kit
```

Generated UF2:

```text
.pio\build\seeed_xiao_nrf52840_kit\firmware-seeed_xiao_nrf52840_kit-<version>.uf2
```

One-command flash helper:

```powershell
& .\Meshtastic\SEEED-XIAO\flash.ps1
```

### RAK4631

[`Meshtastic/RAK4631/README.md`](Meshtastic/RAK4631/README.md)

PlatformIO target:

```text
rak4631
```

Generated UF2:

```text
.pio\build\rak4631\firmware-rak4631-<version>.uf2
```

After double-tapping RESET to expose the RAK UF2 bootloader drive, one-command flash helper:

```powershell
& .\Meshtastic\RAK4631\flash.ps1
```

## Why the rest of this repository still exists

This repository is based on the full Meshtastic firmware source tree. Directories such as `src/`, `variants/`, `bin/`, `lib/`, and the PlatformIO configuration are required to compile working firmware, so they should not be rearranged just to make GitHub look smaller.

The custom HOBO project documentation and normal operating commands are therefore collected under `Meshtastic/`, while the actual compiled source remains in the locations expected by Meshtastic/PlatformIO.

## Local Windows repository

Current working location:

```text
C:\Meshtastic\HOBO\firmware
```

To sync the production branch:

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

For protocol/source details, see [`Meshtastic/SHARED-HOBO/README.md`](Meshtastic/SHARED-HOBO/README.md).
