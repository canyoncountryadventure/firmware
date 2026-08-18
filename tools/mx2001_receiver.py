import argparse
import csv
import struct
import threading
from datetime import datetime
from pathlib import Path

import meshtastic.serial_interface
from pubsub import pub

CSV_PATH = Path("mx2001_data.csv")


def signed_int8(v):
    return v - 256 if v > 127 else v


def decode_mx2001(payload):
    if not isinstance(payload, (bytes, bytearray)):
        return None
    if len(payload) != 19:
        return None
    if payload[0:2] != b"MX":
        return None

    sequence = struct.unpack_from("<H", payload, 4)[0]
    stage_tenths = struct.unpack_from("<h", payload, 6)[0]
    temp_tenths = struct.unpack_from("<h", payload, 8)[0]
    temp_raw = struct.unpack_from("<H", payload, 10)[0]

    mac = ":".join(f"{b:02X}" for b in payload[12:18])
    ble_rssi = signed_int8(payload[18])

    return {
        "sequence": sequence,
        "stage_ft": stage_tenths / 10.0,
        "temp_f": temp_tenths / 10.0,
        "temp_raw": temp_raw,
        "mac": mac,
        "ble_rssi": ble_rssi,
    }


def ensure_csv():
    if CSV_PATH.exists():
        return

    with CSV_PATH.open("w", newline="") as f:
        csv.writer(f).writerow([
            "timestamp",
            "mesh_source",
            "mx2001_mac",
            "sequence",
            "water_level_ft",
            "temperature_f",
            "temperature_raw",
            "ble_rssi_dbm",
            "lora_rssi_dbm",
            "lora_snr_db",
        ])


def on_receive(packet, interface):
    decoded = packet.get("decoded", {})
    portnum = decoded.get("portnum")

    if portnum not in ("PRIVATE_APP", 256):
        return

    reading = decode_mx2001(decoded.get("payload"))
    if not reading:
        return

    timestamp = datetime.now().astimezone().isoformat(timespec="seconds")
    source = packet.get("fromId", f'!{packet.get("from", 0):08x}')
    rssi = packet.get("rxRssi")
    snr = packet.get("rxSnr")

    print()
    print("=" * 50)
    print("MX2001 DATA RECEIVED")
    print(f"Time:        {timestamp}")
    print(f"Mesh source: {source}")
    print(f"Logger:      {reading['mac']}")
    print(f"Sequence:    {reading['sequence']}")
    print(f"Water level: {reading['stage_ft']:.1f} ft")
    print(f"Temperature: {reading['temp_f']:.1f} F")
    print(f"BLE RSSI:    {reading['ble_rssi']} dBm")
    print(f"LoRa RSSI:   {rssi} dBm")
    print(f"LoRa SNR:    {snr} dB")
    print("=" * 50)

    ensure_csv()

    with CSV_PATH.open("a", newline="") as f:
        csv.writer(f).writerow([
            timestamp,
            source,
            reading["mac"],
            reading["sequence"],
            reading["stage_ft"],
            reading["temp_f"],
            reading["temp_raw"],
            reading["ble_rssi"],
            rssi,
            snr,
        ])


def main():
    parser = argparse.ArgumentParser(description="Decode MX2001 PRIVATE_APP packets from a Meshtastic serial radio")
    parser.add_argument("--port", required=True, help="Receiver radio serial port, e.g. COM5")
    args = parser.parse_args()

    pub.subscribe(on_receive, "meshtastic.receive")

    print(f"Opening receiver on {args.port}")
    print("Waiting for MX2001 packets...")

    interface = meshtastic.serial_interface.SerialInterface(devPath=args.port)

    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        interface.close()


if __name__ == "__main__":
    main()
