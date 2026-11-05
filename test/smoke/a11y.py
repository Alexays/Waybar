#!/usr/bin/env python3
"""Assert on Waybar's accessibility (AT-SPI) tree instead of pixels.

Usage: a11y.py [--timeout SECONDS] [--absent] [--showing] TEXT...

Each TEXT must appear (or, with --absent, must not appear) as a substring of a
label in the running Waybar's tree. A widget with a value (a slider) also
reports it as "value=N". The tree is polled until the timeout, so a
test can assert on a state change it just triggered. Robust to fonts and
anti-aliasing, unlike the golden image. With --showing, only widgets on screen
count: a hidden widget keeps its last label in the tree.
"""
import argparse
import sys
import time

import pyatspi


def collect(acc, out, showing):
    try:
        if (showing and acc.getRole() != pyatspi.ROLE_APPLICATION
                and not acc.getState().contains(pyatspi.STATE_SHOWING)):
            return
    except Exception:
        pass
    try:
        name = acc.name or ""
    except Exception:
        name = ""
    if name:
        out.append(name)
    try:
        out.append(f"value={acc.queryValue().currentValue:g}")
    except Exception:
        pass
    try:
        count = acc.childCount
    except Exception:
        count = 0
    for i in range(count):
        try:
            child = acc.getChildAtIndex(i)
            # A hidden widget keeps its last label (sway/mode outside a mode): skip it
            visible = child is not None and child.getState().contains(pyatspi.STATE_VISIBLE)
        except Exception:
            visible = False
        if visible:
            collect(child, out, showing)


def waybar_texts(showing):
    desktop = pyatspi.Registry.getDesktop(0)
    texts, found = [], False
    for i in range(desktop.childCount):
        app = desktop.getChildAtIndex(i)
        if app is not None and (app.name or "").lower().startswith("waybar"):
            found = True
            collect(app, texts, showing)
    return found, texts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=5)
    parser.add_argument("--absent", action="store_true")
    parser.add_argument("--showing", action="store_true")
    parser.add_argument("texts", nargs="+")
    args = parser.parse_args()

    deadline = time.monotonic() + args.timeout
    while True:
        found, texts = waybar_texts(args.showing)
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
