#!/usr/bin/env python3
"""Assert on Waybar's accessibility (AT-SPI) tree instead of pixels.

Usage: a11y.py [--timeout SECONDS] [--absent] TEXT...

Each TEXT must appear (or, with --absent, must not appear) as a substring of a
label in the running Waybar's tree. The tree is polled until the timeout, so a
test can assert on a state change it just triggered. Robust to fonts and
anti-aliasing, unlike the golden image.
"""
import argparse
import sys
import time

import pyatspi


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


def waybar_texts():
    desktop = pyatspi.Registry.getDesktop(0)
    texts, found = [], False
    for i in range(desktop.childCount):
        app = desktop.getChildAtIndex(i)
        if app is not None and (app.name or "").lower().startswith("waybar"):
            found = True
            collect(app, texts)
    return found, texts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=5)
    parser.add_argument("--absent", action="store_true")
    parser.add_argument("texts", nargs="+")
    args = parser.parse_args()

    deadline = time.monotonic() + args.timeout
    while True:
        found, texts = waybar_texts()
        if args.absent:
            bad = [e for e in args.texts if any(e in t for t in texts)]
        else:
            bad = [e for e in args.texts if not any(e in t for t in texts)]
        if found and not bad:
            what = "absent" if args.absent else "present"
            print(f"✓ AT-SPI: {what}: {args.texts}")
            return 0
        if time.monotonic() >= deadline:
            break
        time.sleep(0.25)

    if not found:
        print("::error::waybar did not appear on the AT-SPI bus")
    elif args.absent:
        print(f"::error::AT-SPI: unexpectedly present: {bad}")
    else:
        print(f"::error::AT-SPI: missing: {bad}")
    print("AT-SPI labels seen on waybar:", texts)
    return 1


if __name__ == "__main__":
    sys.exit(main())
