#!/usr/bin/env python3
"""Write Waybar configs that set every documented module option to bad values.

Each module instance carries exactly one mutated option. Instances are packed
into bars in option order, and every bar config is written to OUTDIR as
batch-NN.json; OUTDIR/batches.tsv lists "file<TAB>instances<TAB>options".

Usage: config_fuzz.py OUTDIR [--only OPTION] [--max-modules N]
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAN = ROOT / "man"

# Module name -> man page stem, for the modules that start with no device or daemon.
MODULES = {
    "clock": "clock", "cpu": "cpu", "cpu_usage": "cpu", "cpu_graph": "cpu-graph",
    "cpu_frequency": "cpu", "load": "load", "memory": "memory", "disk": "disk",
    "idle_inhibitor": "idle-inhibitor", "image": "image", "user": "user",
    "inhibitor": "inhibitor", "custom/f": "custom",
    "battery": "battery", "upower": "upower",
    "power-profiles-daemon": "power-profiles-daemon", "bluetooth": "bluetooth",
    "network": "network", "pulseaudio": "pulseaudio",
    "pulseaudio/slider": "pulseaudio-slider", "jack": "jack", "mpd": "mpd",
    "mpris": "mpris", "tray": "tray",
}

# Options read by AModule / ALabel / AIconLabel / ASlider. Each module reads the
# same base-class code, so they go to one module per base class; per-module
# reads of them are covered by that module's man page.
GENERIC_ON = ["clock", "upower", "pulseaudio/slider", "custom/f", "image"]
GENERIC = [
    "format", "format-alt", "format-alt-click", "interval", "on-click",
    "on-click-middle", "on-click-right", "on-scroll-up", "on-scroll-down",
    "on-update", "tooltip", "tooltip-format", "rotate", "max-length",
    "min-length", "align", "justify", "states", "format-icons", "actions",
    "menu", "menu-file", "menu-actions", "cursor", "expand",
    "smooth-scrolling-threshold", "reverse-scrolling", "disable-on-sleep",
    "swap-icon-label", "icon", "icon-size", "icon-spacing", "fit-height",
    "min", "max", "orientation",
]

MENU_XML = """<?xml version="1.0" encoding="UTF-8"?>
<interface><object class="GtkMenu" id="menu">
<child><object class="GtkMenuItem" id="a"><property name="label">a</property></object></child>
</object></interface>
"""


def deep(depth, wrap):
    v = None
    for _ in range(depth):
        v = wrap(v)
    return v


# Wrong types and edge values, for every option.
VALUES = [
    "", "x", "A" * 10000, 0, -1, 2**63 - 1, -(2**63), 1e300, 0.5,
    True, False, None, [], ["x", 1, None, {}], {}, {"x": {"y": None}},
    deep(200, lambda v: [v]), deep(200, lambda v: {"k": v}),
]
# Malformed format strings, also for every option named *format*.
FORMATS = [
    "\u0000x", "<span>&", "%s%n%x", "{", "}", "{{{", "{unknown}", "{}{}{}{}",
    "{:}", "{0}", "{9}", "{:%Q}", "{:>9999}", "{:.9999}",
    "{icon}{icon:>5}{text:.0}",
]


def man_options(stem):
    """Option names from the CONFIGURATION section of man/waybar-<stem>.5.scd."""
    text = (MAN / f"waybar-{stem}.5.scd").read_text()
    section = text.split("# CONFIGURATION", 1)[1]
    section = re.split(r"^# ", section, maxsplit=1, flags=re.M)[0]
    names = re.findall(r"^\|\[ \*([a-z][a-z0-9_-]*)\*\s*$", section, re.M)
    names += re.findall(r"^\*([a-z][a-z0-9_-]*)\*: \+\+$", section, re.M)
    return names


def drawer_options():
    text = (MAN / "waybar.5.scd.in").read_text()
    return re.findall(r"^\*([a-z][a-z0-9-]*)\*: \+\+$", text, re.M)


# Mutations that break the bar today, as (module, option, JSON value).
KNOWN = {
    ("image", "size", "-1"),  # image: a negative size reaches gdk_pixbuf_loader_set_size
}


def instances(menu_file):
    """Yield (option, name, config) for one mutated option per instance."""
    image = str(ROOT / "test/smoke/reference.png")
    base = {"custom/f": {"exec": "echo hi", "interval": "once"}, "image": {"path": image}}
    # Options only read when a sibling option is set.
    context = {
        "menu-file": {"menu": "on-click", "menu-actions": {"a": "true"}},
        "menu-actions": {"menu": "on-click", "menu-file": menu_file},
        "menu": {"menu-file": menu_file, "menu-actions": {"a": "true"}},
        "format-alt-click": {"format-alt": "{}"},
    }
    for mod, stem in MODULES.items():
        opts = man_options(stem) + (GENERIC if mod in GENERIC_ON else [])
        for opt in dict.fromkeys(opts):
            values = VALUES + FORMATS if "format" in opt else VALUES
            for i, val in enumerate(values):
                if (mod, opt, json.dumps(val)) in KNOWN:
                    continue
                cfg = {**base.get(mod, {}), **context.get(opt, {}), opt: val}
                yield opt, f"{mod}#{opt}-{i}", cfg
    for opt in ["orientation", "modules", "drawer"] + [f"drawer.{d}" for d in drawer_options()]:
        for i, val in enumerate(VALUES):
            cfg = {"modules": ["clock"]}
            if opt.startswith("drawer."):
                cfg["drawer"] = {opt[7:]: val}
            else:
                cfg[opt] = val
            yield opt, f"group/{opt.replace('.', '_')}-{i}", cfg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--only", help="emit the bars of this option only")
    # Startup deadlocks past about 512 modules when pipes are capped at 8 KiB
    # (pipe-user-pages-soft), so the cap counts group children too.
    ap.add_argument("--max-modules", type=int, default=400)
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    menu_file = args.outdir / "menu.xml"
    menu_file.write_text(MENU_XML)

    by_opt = {}
    for opt, name, cfg in instances(str(menu_file)):
        if args.only is None or opt == args.only:
            by_opt.setdefault(opt, []).append((name, cfg))
    if not by_opt:
        raise SystemExit(f"no such option: {args.only}")

    # Pack whole options into bars; an option over the cap is split across bars.
    batches = [([], [], 0)]
    for opt, items in by_opt.items():
        for name, cfg in items:
            opts, cur, size = batches[-1]
            weight = 2 if name.startswith("group/") else 1
            if size + weight > args.max_modules:
                opts, cur, size = [], [], 0
                batches.append((opts, cur, size))
            cur.append((name, cfg))
            if opt not in opts:
                opts.append(opt)
            batches[-1] = (opts, cur, size + weight)

    with open(args.outdir / "batches.tsv", "w") as tsv:
        for n, (opts, items, _) in enumerate(batches):
            bar = {"layer": "top", "position": "top", "height": 30,
                   "modules-left": [name for name, _ in items], "clock": {}}
            bar.update(items)
            path = args.outdir / f"batch-{n:02}.json"
            path.write_text(json.dumps(bar))
            tsv.write(f"{path}\t{len(items)}\t{' '.join(opts)}\n")


if __name__ == "__main__":
    main()
