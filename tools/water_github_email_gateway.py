import argparse
import json
import os
import threading
import time
import urllib.error
import urllib.request
from collections import deque

import meshtastic.serial_interface
from pubsub import pub

DEFAULT_REPO = "canyoncountryadventure/firmware"
DEFAULT_ASSIGNEE = "canyoncountryadventure"

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


def source_id(packet):
    value = packet.get("fromId")
    if isinstance(value, str) and value:
        return value
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
                or sid
            )
    return sid


def message_text(decoded):
    text = decoded.get("text")
    if isinstance(text, str):
        return text.strip()

    payload = decoded.get("payload")
    if isinstance(payload, str):
        return payload.strip()
    if isinstance(payload, (bytes, bytearray)):
        return payload.decode("utf-8", errors="replace").strip()

    return ""


def parse_alert(text):
    if not text.startswith("WATER_ALERT|"):
        return None

    parts = text.split("|")
    if len(parts) < 2:
        return None

    alert = {"type": parts[1].strip().lower()}
    for item in parts[2:]:
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        alert[key.strip().lower()] = value.strip()
    return alert


def issue_title(alert, station):
    kind = alert.get("type", "water")
    pct = alert.get("pct", "?")

    if kind == "threshold":
        threshold = alert.get("threshold", pct)
        return f"Water alert: {station} crossed {threshold}%"
    if kind == "refill":
        return f"Water refill: {station} is {pct}%"
    if kind == "sensor_fault":
        return f"Water sensor fault: {station}"
    if kind == "sensor_recovered":
        return f"Water sensor recovered: {station}"
    return f"Water alert: {station} ({kind})"


def create_github_issue(token, repo, assignee, title, body):
    url = f"https://api.github.com/repos/{repo}/issues"
    payload = {"title": title, "body": body}
    if assignee:
        payload["assignees"] = [assignee]

    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        method="POST",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
            "User-Agent": "meshtastic-water-alert-gateway/1.0",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            result = json.loads(response.read().decode("utf-8"))
            print(f"GitHub issue created: #{result.get('number')} {result.get('html_url')}")
            return True
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        print(f"GitHub HTTP {exc.code}: {detail[:500]}")
    except Exception as exc:
        print(f"GitHub issue creation failed: {exc}")
    return False


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Listen for Meshtastic WATER_ALERT messages and create assigned "
            "GitHub issues. GitHub notification settings provide the email. "
            "No Neon/database write is performed."
        )
    )
    parser.add_argument("--port", required=True, help="Gateway radio serial port, e.g. COM11")
    parser.add_argument("--repo", default=DEFAULT_REPO, help="GitHub owner/repo")
    parser.add_argument("--assignee", default=DEFAULT_ASSIGNEE, help="GitHub username to assign")
    args = parser.parse_args()

    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if not token:
        raise SystemExit(
            "GITHUB_TOKEN is not set. Use a fine-grained token with Issues: Read and write "
            "for the target repository. Do not save the token in this script."
        )

    def on_receive(packet, interface):
        if remember_packet(packet.get("id")):
            return

        decoded = packet.get("decoded") or {}
        portnum = decoded.get("portnum")
        if portnum not in ("TEXT_MESSAGE_APP", 1):
            return

        text = message_text(decoded)
        alert = parse_alert(text)
        if not alert:
            return

        station = source_name(interface, packet)
        title = issue_title(alert, station)
        rx_time = packet.get("rxTime") or int(time.time())

        body = "\n".join(
            [
                "Automatic Meshtastic water-level alert.",
                "",
                f"- Station/node: `{station}`",
                f"- Node ID: `{source_id(packet)}`",
                f"- Alert type: `{alert.get('type', 'unknown')}`",
                f"- Percent full: `{alert.get('pct', 'unknown')}%`",
                f"- Threshold: `{alert.get('threshold', 'n/a')}`",
                f"- Distance: `{alert.get('mm', 'unknown')} mm` / `{alert.get('in', 'unknown')} in`",
                f"- Received Unix time: `{rx_time}`",
                f"- LoRa RSSI: `{packet.get('rxRssi', 'unknown')} dBm`",
                f"- LoRa SNR: `{packet.get('rxSnr', 'unknown')} dB`",
                "",
                "No Neon/database record was created by this gateway.",
                "",
                f"Raw alert: `{text}`",
            ]
        )

        print(f"Water alert received: {text}")
        create_github_issue(token, args.repo, args.assignee, title, body)

    pub.subscribe(on_receive, "meshtastic.receive")

    print("=" * 64)
    print("MESHTASTIC WATER -> GITHUB EMAIL GATEWAY")
    print(f"Serial radio: {args.port}")
    print(f"GitHub repo:  {args.repo}")
    print(f"Assignee:     {args.assignee}")
    print("Storage:      NONE (no Neon/database writes)")
    print("Listens for:  WATER_ALERT|...")
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
