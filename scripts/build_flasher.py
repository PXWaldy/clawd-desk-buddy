#!/usr/bin/env python3
"""Builds the web flasher site into flasher/dist.

The result is what Cloudflare serves under /clawd-desk-buddy/:
the page, the fonts of the web UI, the manifest with the firmware version
and the merged firmware image.

Usage:
    python3 scripts/build_flasher.py                  # local build, version "dev"
    python3 scripts/build_flasher.py --version 1.2.0  # release build

Without --firmware the merged image of the last local build is used
(.pio/build/clawd/firmware.factory.bin).
"""
import argparse
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "flasher" / "public"
DIST = ROOT / "flasher" / "dist"
FONTS = ROOT / "web" / "fonts"
DEFAULT_FIRMWARE = ROOT / ".pio" / "build" / "clawd" / "firmware.factory.bin"

# Cloudflare does not accept single files larger than this
MAX_ASSET_BYTES = 25 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--version", default="dev", help="firmware version shown on the page")
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE,
                        help="merged firmware image (bootloader + partitions + app)")
    args = parser.parse_args()

    if not args.firmware.is_file():
        sys.exit(f"Firmware not found: {args.firmware}\nBuild it first with: pio run")
    if args.firmware.stat().st_size > MAX_ASSET_BYTES:
        sys.exit(f"Firmware is larger than 25 MiB, Cloudflare will not accept it: {args.firmware}")

    # Start clean, so no old file stays online by accident
    if DIST.exists():
        shutil.rmtree(DIST)
    shutil.copytree(SOURCE, DIST)

    (DIST / "fonts").mkdir()
    # Same fonts as the web UI on the device, together with their license texts
    for name in ("inter.woff2", "mono.woff2", "OFL-Inter.txt", "OFL-JetBrainsMono.txt"):
        shutil.copy(FONTS / name, DIST / "fonts" / name)

    shutil.copy(args.firmware, DIST / "firmware.bin")

    manifest_path = DIST / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["version"] = args.version.removeprefix("v")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

    print(f"Flasher built in {DIST.relative_to(ROOT)} (firmware version {manifest['version']})")
    for path in sorted(DIST.rglob("*")):
        if path.is_file():
            print(f"  {path.relative_to(DIST)}  {path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
