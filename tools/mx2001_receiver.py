import argparse
import csv
import getpass
import json
import os
import struct
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime
from pathlib import Path

import meshtastic.serial_interface
from pubsub import pub

CSV_PATH = Path("mx2001_data.csv")
DEFAULT_CLOUD_URL = "https://meshtastic-ecru.vercel.app/api/ingest"
CSV_HEADER = [
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
    "hop_start",
    "hop_limit",
    "hops_used",
    "relay_node_byte",
    "last_relay_id",
    "last_relay_long_name",
    "last_relay_short_name",
    "packet_id",
]

CLOUD_ENABLED = False
CLOUD_URL = DEFAULT_CLOUD_URL
INGEST_KEY = ""
STATION_NAME = ""


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


def hop_count(packet):
    hop_start = packet.get("hopStart")
    hop_limit = packet.get("hopLimit")

    if hop_start is None or hop_limit is None:
        return None

    try:
        return int(hop_start) - int(hop_limit)
    except (TypeError, ValueError):
        return None


def resolve_last_relay(interface, relay_node):
    """Resolve Meshtastic's compact relayNode byte against the local node DB."""
    if relay_node is None:
        return None, None, None

    try:
        relay_byte = int(relay_node) & 0xFF
    except (TypeError, ValueError):
        return None, None, None

    for node_id, node in getattr(interface, "nodes", {}).items():
        if not isinstance(node, dict):
            continue

        node_num = node.get("num")
        if node_num is None:
            continue

        try:
            if (int(node_num) & 0xFF) != relay_byte:
                continue
        except (TypeError, ValueError):
            continue

        user = node.get("user") or {}
        long_name = user.get("longName") or user.get("long_name")
        short_name = user.get("shortName") or user.get("short_name")

        if isinstance(node_id, str) and node_id.startswith("!"):
            full_id = node_id
        else:
            full_id = f"!{int(node_num):08x}"

        return full_id, long_name, short_name

    return None, None, None


def ensure_csv():
    if CSV_PATH.exists():
        try:
            with CSV_PATH.open("r", newline="", encoding="utf-8") as f:
                existing_header = next(csv.reader(f), [])
        except (OSError, UnicodeError):
            existing_header = []

        if existing_header == CSV_HEADER:
            return

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        legacy_path = CSV_PATH.with_name(f"mx2001_data_legacy_{stamp}.csv")
        CSV_PATH.rename(legacy_path)
        print(f"Preserved old CSV as: {legacy_path.resolve()}")

    with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerow(CSV_HEADER)


def upload_to_cloud(packet, reading, source, rssi, snr, hop_start, hop_limit, hops,
                    relay_node, relay_id, relay_long, relay_short, packet_id):
    if not CLOUD_ENABLED:
        return

    temp_c = (reading["temp_f"] - 32.0) * 5.0 / 9.0
    relay_name = relay_long or relay_short
    node_num = packet.get("from")

    payload = {
        "type": "mx2001",
        "timestamp": int(time.time()),
        "from": node_num,
        "sender": source,
        "mesh_source": source,
        "station_name": STATION_NAME or f"MX2001 {reading['mac']}",
        "payload": {
            "water_level_ft": reading["stage_ft"],
            "temperature_f": reading["temp_f"],
            "temperature_c": temp_c,
            "temperature_raw": reading["temp_raw"],
            "logger_mac": reading["mac"],
            "sequence": reading["sequence"],
            "ble_rssi_dbm": reading["ble_rssi"],
            "packet_id": packet_id,
        },
        "radio": {
            "rssi": rssi,
            "snr": snr,
            "hop_start": hop_start,
            "hop_limit": hop_limit,
            "hops_used": hops,
            "hops_away": hops,
            "relay_node": relay_node,
            "relay_id": relay_id,
            "relay_name": relay_name,
            "gateway": source,
        },
    }

    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        CLOUD_URL,
        data=body,
        method="POST",
        headers={
            "Content-Type": "application/json",
            "X-Ingest-Key": INGEST_KEY,
            "User-Agent": "mx2001-meshtastic-receiver/1.0",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=8) as response:
            result = json.loads(response.read().decode("utf-8"))
            reading_id = (result.get("reading") or {}).get("id")
            print(f"Cloud:         STORED{f' (row {reading_id})' if reading_id else ''}")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        print(f"Cloud:         HTTP {exc.code} {detail[:160]}")
    except Exception as exc:
        print(f"Cloud:         upload failed: {exc}")


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
    hop_start = packet.get("hopStart")
    hop_limit = packet.get("hopLimit")
    hops = hop_count(packet)
    relay_node = packet.get("relayNode")
    relay_id, relay_long, relay_short = resolve_last_relay(interface, relay_node)
    packet_id = packet.get("id")

    if relay_node is None:
        relay_byte_text = "unknown"
    else:
        try:
            relay_byte_text = f"0x{int(relay_node) & 0xFF:02X}"
        except (TypeError, ValueError):
            relay_byte_text = str(relay_node)

    if relay_id:
        relay_display = relay_id
        if relay_long and relay_short:
            relay_display += f"  {relay_long} ({relay_short})"
        elif relay_long:
            relay_display += f"  {relay_long}"
        elif relay_short:
            relay_display += f"  ({relay_short})"
    elif hops == 0:
        relay_display = "none (direct packet)"
    else:
        relay_display = f"unresolved ({relay_byte_text})"

    print()
    print("=" * 58)
    print("MX2001 DATA RECEIVED")
    print(f"Time:          {timestamp}")
    print(f"Mesh source:   {source}")
    print(f"Logger:        {reading['mac']}")
    print(f"Sequence:      {reading['sequence']}")
    print(f"Water level:   {reading['stage_ft']:.1f} ft")
    print(f"Temperature:   {reading['temp_f']:.1f} F")
    print(f"BLE RSSI:      {reading['ble_rssi']} dBm")
    print(f"LoRa RSSI:     {rssi} dBm")
    print(f"LoRa SNR:      {snr} dB")
    print(f"Hop start:     {hop_start}")
    print(f"Hop limit:     {hop_limit}")
    print(f"HOPS USED:     {hops if hops is not None else 'unknown'}")
    print(f"Last relay:    {relay_display}")
    print(f"Packet ID:     {packet_id}")

    ensure_csv()

    with CSV_PATH.open("a", newline="", encoding="utf-8") as f:
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
            hop_start,
            hop_limit,
            hops,
            relay_node,
            relay_id,
            relay_long,
            relay_short,
            packet_id,
        ])

    upload_to_cloud(
        packet,
        reading,
        source,
        rssi,
        snr,
        hop_start,
        hop_limit,
        hops,
        relay_node,
        relay_id,
        relay_long,
        relay_short,
        packet_id,
    )
    print("=" * 58)


def main():
    global CLOUD_ENABLED, CLOUD_URL, INGEST_KEY, STATION_NAME

    parser = argparse.ArgumentParser(
        description="Decode MX2001 PRIVATE_APP packets from a Meshtastic serial radio"
    )
    parser.add_argument("--port", required=True, help="Receiver radio serial port, e.g. COM5")
    parser.add_argument(
        "--cloud",
        action="store_true",
        help="Upload each received MX2001 reading to the Vercel/Neon dashboard",
    )
    parser.add_argument(
        "--cloud-url",
        default=DEFAULT_CLOUD_URL,
        help=f"Cloud ingest URL (default: {DEFAULT_CLOUD_URL})",
    )
    parser.add_argument(
        "--station",
        default="MX2001 Bench / Field Test",
        help="Friendly station name stored in the dashboard",
    )
    args = parser.parse_args()

    CLOUD_ENABLED = args.cloud
    CLOUD_URL = args.cloud_url
    STATION_NAME = args.station

    if CLOUD_ENABLED:
        INGEST_KEY = os.environ.get("MESHTASTIC_INGEST_KEY", "").strip()
        if not INGEST_KEY:
            INGEST_KEY = getpass.getpass("Vercel INGEST_KEY (hidden): ").strip()
        if not INGEST_KEY:
            raise SystemExit("Cloud upload requested but no ingest key was supplied.")

    pub.subscribe(on_receive, "meshtastic.receive")

    print("=" * 58)
    print("MX2001 MESHTASTIC RECEIVER")
    print(f"Opening receiver on {args.port}")
    print("Waiting for MX2001 packets...")
    print("Route metadata and readings will be saved to mx2001_data.csv")
    if CLOUD_ENABLED:
        print(f"Cloud upload:   ON -> {CLOUD_URL}")
        print(f"Station:        {STATION_NAME}")
    else:
        print("Cloud upload:   OFF (add --cloud to enable)")
    print("Press Ctrl+C to stop.")
    print("=" * 58)

    interface = meshtastic.serial_interface.SerialInterface(devPath=args.port)

    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        interface.close()


if __name__ == "__main__":
    main()
