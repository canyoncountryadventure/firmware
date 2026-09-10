# Archived Branches

Retired development branches were preserved as immutable Git tags before branch cleanup. These tags are historical recovery points, not current deployment targets.

| Retired branch | Archive tag |
|---|---|
| `chatgpt-hobo-mesh-uf2-build` | `archive/chatgpt-hobo-mesh-uf2-build-9a6ef9d` |
| `chatgpt-rak4631-hobo-mesh-uf2` | `archive/chatgpt-rak4631-hobo-mesh-uf2-9cf2537` |
| `CCA-MX-HOBO-PIR-SEEED-v1-ci` | `archive/cca-mx-hobo-pir-seeed-v1-ci` |
| `hobo-mx2001-mx2201-mx2203-rak4631` | `archive/hobo-mx2001-mx2201-mx2203-rak4631` |
| `hobo-universal-validated-2026-08-19` | `archive/hobo-universal-validated-2026-08-19` |
| `hobo-universal-test` | `archive/hobo-universal-test` |
| `hobo-mx2001` | `archive/hobo-mx2001` |
| `hobo-mx2201` | `archive/hobo-mx2201` |
| `hobo-mx2203` | `archive/hobo-mx2203` |
| `hobo-mx2201-mx2001` | `archive/hobo-mx2201-mx2001` |
| `mx2001-integration` | `archive/mx2001-integration` |
| `mx2201-integration` | `archive/mx2201-integration` |
| `mx2201-newread-test` | `archive/mx2201-newread-test` |
| `mx2203-discovery-test` | `archive/mx2203-discovery-test` |
| `rak4631-mx2201-raw-debug` | `archive/rak4631-mx2201-raw-debug` |
| `heltec-home-http-gateway` | `archive/heltec-home-http-gateway` |
| `heltec-home-http-gateway-hidden-valley` | `archive/heltec-home-http-gateway-hidden-valley` |
| `heltec-home-http-gateway-rock` | `archive/heltec-home-http-gateway-rock` |

## Active branches after cleanup

- `hobo-mx2001-mx2201-mx2203` — canonical universal HOBO field-node firmware
- `cca-heltec-sensor-gateway` — canonical production Heltec gateway
- `CCA-MX-HOBO-PIR-SEEED-v1` — HOBO + PIR field variant
- `CCA-MX-HOBO-PIR-ROCK-SEEED-v1` — HOBO + PIR + rock/soil field variant
- `fucking-around` — experimental ultrasonic/water field-node work
- `fucking-around-heltec` — paired experimental water-alert gateway
- `trail-sen0171` — dedicated trail-counter experiment

The canonical HOBO production branch supports RAK4631 and Seeed targets; the retired `hobo-mx2001-mx2201-mx2203-rak4631` branch must not be used as a production shortcut.
