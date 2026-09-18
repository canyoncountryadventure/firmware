#!/usr/bin/env python3
"""
Stream a standard RAK4631 Nordic Legacy DFU OTA ZIP through the Phase-2
RAK4631 DFU Scout over USB CDC.

Usage:
  python tools/rak_dfu_serial_upload.py COM45 firmware-rak4631-...-ota.zip

Requires:
  pip install pyserial
"""

import argparse
import json
import sys
import time
import zipfile

try:
    import serial
except ImportError:
    print("ERROR: pyserial is required. Install with: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def load_bundle(path):
    with zipfile.ZipFile(path, "r") as zf:
        try:
            manifest = json.loads(zf.read("manifest.json"))
        except KeyError:
            raise RuntimeError("manifest.json missing from OTA ZIP")

        root = manifest.get("manifest", {})
        app = root.get("application")
        if not isinstance(app, dict):
            raise RuntimeError("This Phase-2 uploader supports application-only OTA ZIPs")

        bin_name = app.get("bin_file")
        dat_name = app.get("dat_file")
        if not bin_name or not dat_name:
            raise RuntimeError("manifest application entry is missing bin_file or dat_file")

        fw = zf.read(bin_name)
        dat = zf.read(dat_name)

    if not dat or len(dat) > 2048:
        raise RuntimeError(f"Unexpected init packet size: {len(dat)} bytes")
    if not fw or len(fw) > 900000:
        raise RuntimeError(f"Unexpected firmware size: {len(fw)} bytes")

    return dat, fw, bin_name, dat_name


def send_line(ser, text):
    ser.write((text + "\n").encode("ascii"))
    ser.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="Scout USB serial port, e.g. COM45")
    ap.add_argument("ota_zip", help="RAK4631 application OTA ZIP")
    ap.add_argument("--ready-timeout", type=int, default=60,
                    help="Seconds to wait for Scout to have AdaDFU connected")
    args = ap.parse_args()

    try:
        dat, fw, bin_name, dat_name = load_bundle(args.ota_zip)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    print(f"OTA bundle: {args.ota_zip}")
    print(f"  DAT: {dat_name} ({len(dat)} bytes)")
    print(f"  BIN: {bin_name} ({len(fw)} bytes)")
    print(f"Opening Scout on {args.port}...")

    with serial.Serial(args.port, 115200, timeout=1.0, write_timeout=15) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()

        deadline = time.monotonic() + args.ready_timeout
        ready = False

        while time.monotonic() < deadline:
            send_line(ser, "STATUS")
            probe_until = time.monotonic() + 1.5
            while time.monotonic() < probe_until:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip()
                if line:
                    print(f"[SCOUT] {line}")
                if line == "DFU_STATUS READY":
                    ready = True
                    break
            if ready:
                break

        if not ready:
            print("ERROR: Scout did not report DFU_STATUS READY.", file=sys.stderr)
            print("Put the target into AdaDFU mode and keep the Scout near it.", file=sys.stderr)
            return 3

        print("Scout is connected to AdaDFU. Starting transfer.")
        send_line(ser, f"UPLOAD {len(dat)} {len(fw)}")

        last_activity = time.monotonic()

        while True:
            raw = ser.readline()
            if not raw:
                if time.monotonic() - last_activity > 150:
                    print("ERROR: no Scout output for 150 seconds", file=sys.stderr)
                    return 4
                continue

            last_activity = time.monotonic()
            line = raw.decode("utf-8", errors="replace").rstrip()
            if not line:
                continue

            print(f"[SCOUT] {line}")

            if line.startswith("REQ "):
                parts = line.split()
                if len(parts) != 4:
                    print(f"ERROR: malformed REQ line: {line}", file=sys.stderr)
                    return 5

                _, kind, off_s, want_s = parts
                off = int(off_s)
                want = int(want_s)

                if kind == "DAT":
                    src = dat
                elif kind == "BIN":
                    src = fw
                else:
                    print(f"ERROR: unknown request type {kind}", file=sys.stderr)
                    return 5

                end = off + want
                if off < 0 or want <= 0 or end > len(src):
                    print(f"ERROR: out-of-range request {kind} {off} {want}", file=sys.stderr)
                    return 5

                ser.write(src[off:end])
                ser.flush()
                continue

            if line.startswith("DFU SUCCESS:"):
                print("SUCCESS: target accepted the firmware and rebooted.")
                return 0

            if line.startswith("DFU ERROR:"):
                print("TRANSFER FAILED. The target should be reset/recovered before retrying.", file=sys.stderr)
                return 6


if __name__ == "__main__":
    raise SystemExit(main())
