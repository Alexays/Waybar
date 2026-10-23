# Wiki sync

The scdoc man pages under [`man/`](../../man) are the **single source of truth**
for module documentation. The GitHub wiki module pages are generated from them by
[`generate.py`](generate.py) and kept in sync automatically by the
[`wiki.yml`](../workflows/wiki.yml) workflow on every push to `master` that
touches `man/**` or this tooling.

## Files

- `mapping.json` — maps each wiki page to the ordered list of man-page basenames
  that compose it. Several man pages are concatenated into one aggregated page
  (e.g. `Module:-Hyprland` ← the four `waybar-hyprland-*` pages). **Add an entry
  here when you add a new man page** — the workflow fails the mapping check
  otherwise.
- `generate.py` — `scdoc → roff → pandoc → gfm`, strips the `NAME`/`FILES`/`AUTHOR`
  man sections, and writes one `Module:-*.md` per mapping entry.
- `extras/<Page>.md` — optional hand-maintained appendix (screenshots, showcase
  snippets that have no man equivalent), appended verbatim after the generated body.

The generator also keeps `_Sidebar.md` in sync: any `Module:-*` page in the mapping
that is not yet linked in the sidebar is inserted alphabetically into the `Modules:`
list. Existing sidebar entries (custom labels, nested sub-entries, hand-written
non-module links) are never modified — so a new module auto-appears in navigation
without disturbing the curated structure.

## What it touches

Only the pages listed in `mapping.json` are (re)written. Every other wiki page
(`Home`, `Installation`, user showcases, the hand-written `Module:-Cava:-GLSL`,
`Module:-Group`, `Module:-Load`, …) is left untouched.

## To edit a module's docs

Edit the man page under `man/`, not the wiki. The wiki page is overwritten on the
next sync. Put anything with no man equivalent (images, etc.) in `extras/`.

## Run locally

```sh
python3 .github/wiki/generate.py --check                 # validate mapping only
python3 .github/wiki/generate.py --out-dir /tmp/wiki-out # generate a preview
```

## One-time setup

The repository wiki must be enabled (Settings → Features → Wikis) with at least
one initial page so the `.wiki.git` remote exists.
