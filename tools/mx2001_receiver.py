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

    # relayNode is a compact one-byte identifier. Match it to the low byte
    # of node numbers already known by this receiving radio.
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
        return

    with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
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
            "hop_start",
            "hop_limit",
            "hops_used",
            "relay_node_byte",
            "last_relay_id",
            "last_relay_long_name",
            "last_relay_short_name",
            "packet_id",
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
    print("=" * 58)

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


def main():
    parser = argparse.ArgumentParser(
        description="Decode MX2001 PRIVATE_APP packets from a Meshtastic serial radio"
    )
    parser.add_argument("--port", required=True, help="Receiver radio serial port, e.g. COM5")
    args = parser.parse_args()

    pub.subscribe(on_receive, "meshtastic.receive")

    print("=" * 58)
    print("MX2001 MESHTASTIC RECEIVER")
    print(f"Opening receiver on {args.port}")
    print("Waiting for MX2001 packets...")
    print("Route metadata and readings will be saved to mx2001_data.csv")
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
