# Meshtastic HOBO Project

This folder is the **user-facing project structure** for the HOBO → Meshtastic work.

You normally do **not** need to browse the rest of the upstream Meshtastic firmware tree. The underlying `src/`, `variants/`, `bin/`, and other directories are retained because PlatformIO needs them to compile the firmware.

## Production branch

Use only:

```text
hobo-mx2001-mx2201-mx2203
```

This is the canonical hardware-validated branch for both supported radios.

## Project layout

```text
Meshtastic/
├── README.md
├── SEEED-XIAO/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── RAK4631/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── SHARED-HOBO/
│   └── README.md
└── ARCHIVE/
    └── README.md
```

## Choose the radio

### Seeed XIAO nRF52840 + Wio-SX1262

Go to:

[`SEEED-XIAO/README.md`](SEEED-XIAO/README.md)

PlatformIO target:

```text
seeed_xiao_nrf52840_kit
```

Generated UF2 location:

```text
.pio\build\seeed_xiao_nrf52840_kit\firmware-seeed_xiao_nrf52840_kit-<version>.uf2
```

### RAK4631 / RAK19003

Go to:

[`RAK4631/README.md`](RAK4631/README.md)

PlatformIO target:

```text
rak4631
```

Generated UF2 location:

```text
.pio\build\rak4631\firmware-rak4631-<version>.uf2
```

`<version>` changes with the firmware version/commit, so use the newest `.uf2` in the target build directory rather than relying on an old filename.

## Supported HOBO loggers

| Logger | Live `READ` result |
|---|---|
| MX2001 | Water level + temperature |
| MX2201 | Temperature |
| MX2203 | Temperature |

The same universal HOBO protocol implementation is used on both radios.

See [`SHARED-HOBO/README.md`](SHARED-HOBO/README.md) for protocol, source files, decoder details, and validation notes.

## Generated firmware files are local

The `.pio` directory is a PlatformIO build directory on your computer. Generated `.uf2` files are **not committed to GitHub**.

On your Windows machine the repository is currently used from:

```text
C:\Meshtastic\HOBO\firmware
```

So a RAK build file will be under:

```text
C:\Meshtastic\HOBO\firmware\.pio\build\rak4631\
```

and a Seeed build file will be under:

```text
C:\Meshtastic\HOBO\firmware\.pio\build\seeed_xiao_nrf52840_kit\
```

## Old branches

Old integration, discovery, model-specific, validation, and raw-debug branches are retained only for rollback and protocol history. Do not use them for a normal deployment.

See [`ARCHIVE/README.md`](ARCHIVE/README.md).
