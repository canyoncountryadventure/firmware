import argparse
import getpass
import json
import os
import struct
import threading
import time
import urllib.error
import urllib.request
from collections import deque

import meshtastic.serial_interface
from pubsub import pub

DEFAULT_CLOUD_URL = "https://meshtastic-ecru.vercel.app/api/ingest"

CLOUD_URL = DEFAULT_CLOUD_URL
INGEST_KEY = ""
GATEWAY_NAME = "Slickrock Hydro / laptop-python-bridge"

_seen_packet_ids = set()
_seen_packet_order = deque(maxlen=512)


def remember_packet(packet_id):
    if packet_id is None:
        return False
    try:
        packet_id = int(packet_id)
    except (TypeError, ValueError):
        return False
    if packet_id in _seen_packet_ids:
        return True
    if len(_seen_packet_order) == _seen_packet_order.maxlen:
        old = _seen_packet_order.popleft()
        _seen_packet_ids.discard(old)
    _seen_packet_order.append(packet_id)
    _seen_packet_ids.add(packet_id)
    return False


def signed_int8(value):
    return value - 256 if value > 127 else value


def decode_mx2001(payload):
    if not isinstance(payload, (bytes, bytearray)) or len(payload) != 19:
        return None
    if payload[0:2] != b"MX":
        return None

    sequence = struct.unpack_from("<H", payload, 4)[0]
    stage_tenths = struct.unpack_from("<h", payload, 6)[0]
    temp_tenths = struct.unpack_from("<h", payload, 8)[0]
    temp_raw = struct.unpack_from("<H", payload, 10)[0]
    mac = ":".join(f"{b:02X}" for b in payload[12:18])

    return {
        "water_level_ft": stage_tenths / 10.0,
        "temperature_f": temp_tenths / 10.0,
        "temperature_c": ((temp_tenths / 10.0) - 32.0) * 5.0 / 9.0,
        "temperature_raw": temp_raw,
        "logger_mac": mac,
        "sequence": sequence,
        "ble_rssi_dbm": signed_int8(payload[18]),
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


def source_id(packet):
    source = packet.get("fromId")
    if isinstance(source, str) and source:
        return source
    try:
        return f"!{int(packet.get('from', 0)):08x}"
    except (TypeError, ValueError):
        return "!00000000"


def source_name(interface, packet):
    sid = source_id(packet)
    node = getattr(interface, "nodes", {}).get(sid, {})
    if isinstance(node, dict):
        user = node.get("user") or {}
        if isinstance(user, dict):
            return (
                user.get("longName")
                or user.get("long_name")
                or user.get("shortName")
                or user.get("short_name")
            )
    return f"Meshtastic {sid[-4:]}"


def radio_metadata(packet):
    return {
        "rssi": packet.get("rxRssi"),
        "snr": packet.get("rxSnr"),
        "hop_start": packet.get("hopStart"),
        "hop_limit": packet.get("hopLimit"),
        "hops_away": hop_count(packet),
        "relay_node": packet.get("relayNode"),
        "channel": packet.get("channel"),
        "gateway": GATEWAY_NAME,
    }


def upload(body):
    request = urllib.request.Request(
        CLOUD_URL,
        data=json.dumps(body).encode("utf-8"),
        method="POST",
        headers={
            "Content-Type": "application/json",
            "X-Ingest-Key": INGEST_KEY,
            "User-Agent": "hobo-meshtastic-cloud-gateway/1.0",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            result = json.loads(response.read().decode("utf-8"))
            reading = result.get("reading") or {}
            print(f"Cloud: STORED row={reading.get('id', 'unknown')}")
            return True
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        print(f"Cloud: HTTP {exc.code} {detail[:200]}")
    except Exception as exc:
        print(f"Cloud: upload failed: {exc}")
    return False


def handle_mx2001(packet, interface, decoded):
    reading = decode_mx2001(decoded.get("payload"))
    if not reading:
        return False

    node_num = packet.get("from")
    sid = source_id(packet)
    station = source_name(interface, packet)

    print()
    print("=" * 64)
    print("MX2001 DATA")
    print(f"Source:      {sid}  {station}")
    print(f"Logger:      {reading['logger_mac']}")
    print(f"Level:       {reading['water_level_ft']:.1f} ft")
    print(f"Temperature: {reading['temperature_f']:.1f} F")
    print(f"BLE RSSI:    {reading['ble_rssi_dbm']} dBm")
    print(f"LoRa RSSI:   {packet.get('rxRssi')} dBm")
    print(f"LoRa SNR:    {packet.get('rxSnr')} dB")
    print(f"Hops:        {hop_count(packet)}")

    body = {
        "type": "mx2001",
        "timestamp": int(packet.get("rxTime") or time.time()),
        "from": node_num,
        "station_name": station,
        "payload": reading,
        "radio": radio_metadata(packet),
    }
    upload(body)
    print("=" * 64)
    return True


def handle_environment(packet, interface, decoded):
    telemetry = decoded.get("telemetry") or {}
    if not isinstance(telemetry, dict):
        return False

    env = telemetry.get("environmentMetrics") or telemetry.get("environment_metrics") or {}
    if not isinstance(env, dict):
        return False

    temperature = env.get("temperature")
    if temperature is None:
        return False

    try:
        temperature = float(temperature)
    except (TypeError, ValueError):
        return False

    node_num = packet.get("from")
    sid = source_id(packet)
    station = source_name(interface, packet)
    observed = telemetry.get("time") or packet.get("rxTime") or int(time.time())

    print()
    print("=" * 64)
    print("ENVIRONMENT TELEMETRY")
    print(f"Source:      {sid}  {station}")
    print(f"Temperature: {temperature:.2f} C / {(temperature * 9.0 / 5.0 + 32.0):.2f} F")
    print(f"LoRa RSSI:   {packet.get('rxRssi')} dBm")
    print(f"LoRa SNR:    {packet.get('rxSnr')} dB")
    print(f"Hops:        {hop_count(packet)}")

    body = {
        "type": "telemetry",
        "timestamp": int(observed),
        "from": node_num,
        "station_name": station,
        "payload": {"temperature": temperature},
        "radio": radio_metadata(packet),
    }
    upload(body)
    print("=" * 64)
    return True


def on_receive(packet, interface):
    packet_id = packet.get("id")
    if remember_packet(packet_id):
        return

    decoded = packet.get("decoded") or {}
    portnum = decoded.get("portnum")

    if portnum in ("PRIVATE_APP", 256):
        handle_mx2001(packet, interface, decoded)
        return

    if portnum in ("TELEMETRY_APP", 67):
        handle_environment(packet, interface, decoded)


def main():
    global CLOUD_URL, INGEST_KEY, GATEWAY_NAME

    parser = argparse.ArgumentParser(
        description="Receive HOBO telemetry from a Meshtastic gateway radio and upload to Vercel/Neon"
    )
    parser.add_argument("--port", required=True, help="Gateway radio serial port, e.g. COM11")
    parser.add_argument("--cloud-url", default=DEFAULT_CLOUD_URL)
    parser.add_argument("--gateway-name", default=GATEWAY_NAME)
    args = parser.parse_args()

    CLOUD_URL = args.cloud_url
    GATEWAY_NAME = args.gateway_name

    INGEST_KEY = os.environ.get("MESHTASTIC_INGEST_KEY", "").strip()
    if not INGEST_KEY:
        INGEST_KEY = getpass.getpass("Vercel INGEST_KEY (hidden): ").strip()
    if not INGEST_KEY:
        raise SystemExit("No ingest key supplied")

    pub.subscribe(on_receive, "meshtastic.receive")

    print("=" * 64)
    print("UNIVERSAL HOBO MESHTASTIC CLOUD GATEWAY")
    print(f"Serial radio: {args.port}")
    print(f"Cloud:        {CLOUD_URL}")
    print(f"Gateway:      {GATEWAY_NAME}")
    print("Accepts:      MX2001 PRIVATE_APP + MX2201/MX2203 TELEMETRY_APP")
    print("Press Ctrl+C to stop")
    print("=" * 64)

    interface = meshtastic.serial_interface.SerialInterface(devPath=args.port)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        interface.close()


if __name__ == "__main__":
    main()
