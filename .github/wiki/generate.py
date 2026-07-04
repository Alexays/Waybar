#!/usr/bin/env python3
"""Generate GitHub wiki pages from the scdoc man pages.

The man pages under man/ are the single source of truth for module
documentation. This script converts them to GitHub-flavoured Markdown
(scdoc -> roff -> pandoc -> gfm) and writes one wiki page per entry in
mapping.json, concatenating several man pages into one page where the
wiki keeps an aggregated page (e.g. Module:-Hyprland).

Only the pages listed in mapping.json are (re)written; every other wiki
page (Home, Installation, user showcases, ...) is left untouched.

Usage:
    generate.py --man-dir man --out-dir <wiki-checkout> [--check]

Requires: scdoc, pandoc.
"""
import argparse
import json
import os
import re
import subprocess
import sys

# Man-page sections that are meaningless on the wiki and must be dropped.
DROP_SECTIONS = {"NAME", "FILES", "AUTHOR", "AUTHORS"}

HEADING_RE = re.compile(r"^(#+)\s+(.*)$")


def sh(cmd, stdin=None):
    return subprocess.run(
        cmd, input=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=True,
    ).stdout


DEF_START = re.compile(r"^\s*\*(.+?)\*:\s*(?:\+\+)?\s*$")
INDENT = re.compile(r"^(?:\t| {2,})")
FIELD = re.compile(r"^(typeof|default)\s*:\s*(.*)$", re.IGNORECASE)


def normalize_option_blocks(text):
    """Rewrite definition-style option blocks into scdoc table syntax.

    Some man pages document options as:

        *name*: ++
            typeof: string ++
            default: foo ++
            Description...

    which pandoc renders as a flat wall of text. Convert consecutive such
    blocks into the same scdoc table syntax the table-style pages already use
    (Option / Typeof / Default / Description), so every page renders as a
    clean table. Table-style pages are left untouched (they never match).
    Nested (indented) sub-option blocks become their own table. Fenced code
    blocks are passed through verbatim.
    """
    lines = text.split("\n")
    out, i, n, in_code = [], 0, len(lines), False
    while i < n:
        if lines[i].lstrip().startswith("```"):
            in_code = not in_code
            out.append(lines[i])
            i += 1
            continue
        # A block starts with `*name*:` and its first indented line is `typeof:`.
        m = DEF_START.match(lines[i])
        nxt = lines[i + 1] if i + 1 < n else ""
        if in_code or not (m and INDENT.match(nxt)
                           and nxt.strip().lower().startswith("typeof")):
            out.append(lines[i])
            i += 1
            continue
        rows = []
        while i < n and DEF_START.match(lines[i]):
            name = DEF_START.match(lines[i]).group(1)
            i += 1
            typ = default = ""
            desc = []
            while i < n and INDENT.match(lines[i]):
                cell = re.sub(r"\s*\+\+\s*$", "", lines[i].strip())
                f = FIELD.match(cell)
                if f and f.group(1).lower() == "typeof":
                    typ = f.group(2).strip()
                elif f and f.group(1).lower() == "default":
                    default = f.group(2).strip()
                elif cell:
                    desc.append(cell)
                i += 1
            rows.append((name, typ, default, " ".join(desc)))
            if i < n and lines[i].strip() == "" and i + 1 < n and DEF_START.match(lines[i + 1]):
                i += 1  # swallow blank line between two blocks, continue the run
        out += ["[- *Option*", ":- *Typeof*", ":- *Default*", ":- *Description*"]
        for name, typ, default, desc in rows:
            out += [f"|[ *{name}*", f":[ {typ}", f":[ {default}", f":[ {desc}"]
        out.append("")
    return "\n".join(out)


def man_to_gfm(scd_path):
    """scdoc -> roff -> pandoc gfm, as raw markdown text."""
    src = normalize_option_blocks(open(scd_path, encoding="utf-8").read())
    roff = sh(["scdoc"], stdin=src.encode("utf-8"))
    gfm = sh(["pandoc", "-f", "man", "-t", "gfm", "--wrap=none"], stdin=roff)
    return gfm.decode("utf-8")


def strip_sections(md):
    """Remove top-level man-only sections (NAME/FILES/AUTHOR)."""
    out, drop = [], False
    for line in md.splitlines():
        m = HEADING_RE.match(line)
        if m and len(m.group(1)) == 1:
            drop = m.group(2).strip().upper() in DROP_SECTIONS
        if not drop:
            out.append(line)
    return "\n".join(out).strip("\n")


def demote(md, levels=1):
    """Add `levels` extra '#' to every heading (for aggregated pages)."""
    out = []
    for line in md.splitlines():
        m = HEADING_RE.match(line)
        out.append("#" * levels + line if m else line)
    return "\n".join(out)


EMPTY_ROW = re.compile(r"^\|(?:\s*\|)+\s*$")
SEP_ROW = re.compile(r"^\|[\s:\-|]+\|\s*$")


def fix_tables(md):
    """Drop pandoc's empty leading header row so the real first row is the header.

    scdoc's `[-` header markers make pandoc emit an empty header row followed by
    the actual header as the first body row. Detect `|  |  |` + separator and
    remove them, promoting the next row to the header with the same alignment.
    """
    lines = md.splitlines()
    out, i = [], 0
    while i < len(lines):
        if (EMPTY_ROW.match(lines[i]) and i + 2 < len(lines)
                and SEP_ROW.match(lines[i + 1]) and lines[i + 2].startswith("|")):
            out.append(lines[i + 2])       # real header
            out.append(lines[i + 1])       # reuse the separator (keeps alignment)
            i += 3
        else:
            out.append(lines[i])
            i += 1
    return "\n".join(out)


def pretty(basename):
    """waybar-sway-mode -> sway/mode (submodule heading for aggregated pages)."""
    return basename.removeprefix("waybar-").replace("-", "/", 1)


REPO = "https://github.com/Alexays/Waybar"


def build_page(man_dir, sources, page, extras_dir):
    parts = []
    srclinks = ", ".join(f"[`man/{s}.5.scd`]({REPO}/blob/master/man/{s}.5.scd)"
                         for s in sources)
    note = ("> [!NOTE]\n"
            f"> This page is **auto-generated from {srclinks}** on the `master` branch.\n"
            "> Do not edit it here — changes will be overwritten on the next sync.\n"
            "> To update it, edit the man page(s) and open a PR.")
    parts.append(note)
    aggregated = len(sources) > 1
    for src in sources:
        scd = os.path.join(man_dir, src + ".5.scd")
        body = fix_tables(strip_sections(man_to_gfm(scd)))
        if aggregated:
            parts.append(f"\n# {pretty(src)}\n\n" + demote(body))
        else:
            parts.append("\n" + body)
    # Optional hand-maintained appendix (screenshots, showcase snippets) that
    # has no man-page equivalent: .github/wiki/extras/<Page>.md, appended verbatim.
    extra = os.path.join(extras_dir, page + ".md")
    if os.path.exists(extra):
        parts.append("\n" + open(extra).read().strip("\n"))
    return "\n".join(parts).rstrip() + "\n"


ENTRY_RE = re.compile(r"^    - \[([^\]]+)\]\(\./Module:-")
LINK_RE = re.compile(r"\]\(\./(Module:-[^)]+)\)")


def sync_sidebar(out_dir, mapping):
    """Non-destructively add any mapped Module page missing from _Sidebar.md.

    Only inserts links for pages absent from the sidebar; existing entries
    (custom labels, nested sub-entries, hand-written non-module links) are
    never modified. Insertion is alphabetical within the top-level module list.
    """
    path = os.path.join(out_dir, "_Sidebar.md")
    if not os.path.exists(path):
        print("  no _Sidebar.md; skipping sidebar sync")
        return
    lines = open(path).read().splitlines()
    linked = set(LINK_RE.findall("\n".join(lines)))
    missing = sorted((p for p in mapping
                      if p.startswith("Module:-") and p not in linked),
                     key=str.lower)
    if not missing:
        print("  sidebar up to date")
        return

    def entries():
        return [i for i, l in enumerate(lines) if ENTRY_RE.match(l)]

    if not entries():
        print("  could not locate Modules section; skipping sidebar sync")
        return
    for page in missing:
        label = page[len("Module:-"):].replace("-", " ")
        new_line = f"    - [{label}](./{page})"
        pos = None
        for i in entries():
            if ENTRY_RE.match(lines[i]).group(1).lower() > label.lower():
                pos = i
                break
        if pos is None:  # after the last module entry and its sub-entries
            j = entries()[-1] + 1
            while j < len(lines) and lines[j].startswith("        "):
                j += 1
            pos = j
        lines.insert(pos, new_line)
        print(f"  sidebar += {label}")
    open(path, "w").write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--man-dir", default="man")
    ap.add_argument("--out-dir")
    ap.add_argument("--mapping", default=os.path.join(os.path.dirname(__file__), "mapping.json"))
    ap.add_argument("--check", action="store_true",
                    help="Only validate the mapping vs man/, write nothing.")
    args = ap.parse_args()

    mapping = {k: v for k, v in json.load(open(args.mapping)).items()
               if not k.startswith("_")}

    # Validate: every man page mapped exactly once; every source exists.
    mapped, errors = {}, []
    for page, sources in mapping.items():
        for s in sources:
            if not os.path.exists(os.path.join(args.man_dir, s + ".5.scd")):
                errors.append(f"{page}: man/{s}.5.scd does not exist")
            if s in mapped:
                errors.append(f"{s} mapped to both {mapped[s]} and {page}")
            mapped[s] = page
    on_disk = {f[:-6] for f in os.listdir(args.man_dir) if f.endswith(".5.scd")}
    for s in sorted(on_disk - set(mapped)):
        errors.append(f"man/{s}.5.scd is not referenced in mapping.json")
    if errors:
        print("Mapping errors:\n  " + "\n  ".join(errors), file=sys.stderr)
        return 1
    print(f"Mapping OK: {len(on_disk)} man pages -> {len(mapping)} wiki pages")
    if args.check:
        return 0
    if not args.out_dir:
        print("--out-dir is required unless --check", file=sys.stderr)
        return 2

    extras_dir = os.path.join(os.path.dirname(args.mapping), "extras")
    os.makedirs(args.out_dir, exist_ok=True)
    for page, sources in mapping.items():
        out = os.path.join(args.out_dir, page + ".md")
        open(out, "w").write(build_page(args.man_dir, sources, page, extras_dir))
        print(f"  wrote {page}.md  ({', '.join(sources)})")
    sync_sidebar(args.out_dir, mapping)
    return 0


if __name__ == "__main__":
    sys.exit(main())
