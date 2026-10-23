#!/usr/bin/env python3
"""Tier 2 — assert on Waybar's accessibility (AT-SPI) tree instead of pixels.

Walks the running Waybar's GTK accessibility tree and checks that the expected
module labels are exposed. This is robust to fonts/anti-aliasing (unlike the
golden image) and verifies actual widget content.
"""
import sys

import pyatspi

EXPECT = ["Waybar", "CI smoke test", "12:34"]


def collect(acc, out):
    try:
        name = acc.name or ""
    except Exception:
        name = ""
    if name:
        out.append(name)
    try:
        count = acc.childCount
    except Exception:
        count = 0
    for i in range(count):
        try:
            child = acc.getChildAtIndex(i)
        except Exception:
            child = None
        if child is not None:
            collect(child, out)


def main():
    desktop = pyatspi.Registry.getDesktop(0)
    texts, found = [], False
    for i in range(desktop.childCount):
        app = desktop.getChildAtIndex(i)
        if app is None:
            continue
        if (app.name or "").lower().startswith("waybar"):
            found = True
            collect(app, texts)
    print("AT-SPI labels seen on waybar:", texts)
    if not found:
        print("ERROR: waybar did not appear on the AT-SPI bus")
        return 1
    missing = [e for e in EXPECT if not any(e in t for t in texts)]
    if missing:
        print("ERROR: expected labels missing from AT-SPI tree:", missing)
        return 1
    print("OK: all expected labels present via AT-SPI")
    return 0


if __name__ == "__main__":
    sys.exit(main())
