# Mineral Icons (Claude): focused repair of 4802276c — handoff

Repair candidate commit `d8c246be53e741756320dbb8d10be7c8cba70b08` on this
worktree (`/home/cabewse/work_SPaC3/container-wm/.cache/qinda-icons`, branch
`codex/qinda-icons`), on top of the previously-blocked `4802276c`. Worker
record correction is a separate commit, `8a1d868546650948da1268371a7f56401dbf5592`
(record-only, no catalog/tool changes). This is a focused repair only — no
redesign of the 127-canonical catalog the manager already reviewed.

## 1. BLOCKER fixed: wifi symbolic rendering

`_wifi_level()` in `tools/qinda_icon_catalog_devices.py` was
`dot(32, 48, 3) + "".join(WIFI_ARCS[:bars])` — string-concatenating raw `d=`
path data with no `<path>` wrapper, so it became stray XML text/tail instead
of a paintable element. Every `network-wireless-signal-*-symbolic.svg`
rendered as a bare dot regardless of level, even though the *source* differed
per level (which is why the earlier source-text uniqueness check missed it).

Fixed to `dot(32, 48, 3) + "".join(stroke(a, width=4) for a in WIFI_ARCS[:bars])`.
Confirmed by reading the regenerated SVG directly (now four `<path stroke=...>`
elements instead of a lone `<circle>`), and by two new validator checks
described below. I also reproduced the original failure by temporarily
reverting the fix in this worktree and re-running the validator, confirmed it
was caught, then restored the fix before committing — not claiming a check
"would" work, verified that it does.

## 2. Coverage contract completed

`tests/shell/verify_shell_icon_coverage.cmake` was parsed programmatically
(not by re-reading the manager's five named examples and stopping there) to
extract every literal name any `require_icon(...)` call pins:

```
python3 - <<'PY'
import re
text = open("tests/shell/verify_shell_icon_coverage.cmake").read()
foreach_names = set()
for block in re.findall(r"foreach\(icon_name IN ITEMS\s*(.*?)\)", text, re.S):
    foreach_names.update(block.split())
direct = set(re.findall(r'require_icon\("[^"]+"\s*,?\s*\n?\s*"([^"]+)"\)', text))
names = {n.replace("-symbolic", "") for n in (foreach_names | direct)}
names.discard("${icon_name}")
print(len(names))
PY
```
→ `53`. All 53 now resolve in the catalog (`set(CANON) | set(ALIASES)`).

New/changed names:
- `network-wireless-off` (new canonical) — radio hardware disabled: base dot
  + full diagonal slash, no bars at all.
- `network-wireless-disconnected` (new canonical) — radio on, no association
  yet: base dot + one *dashed* arc (`stroke-dasharray`), distinct from both
  `-off` (no arc, slash) and `-signal-weak` (one solid arc).
- `network-bluetooth-activated` (new alias of `network-bluetooth-active`) —
  same "radio is on" object, just the runtime's spelling; not given new
  geometry since the meaning is identical.
- `view-refresh` (new canonical, actions) — a single circular arrow +
  arrowhead, kept visually distinct from `system-reboot` (same family but
  with an added power tick) and `emblem-synchronized` (a full double-arrow
  loop).
- `dashboard-show` (new canonical, actions) — an asymmetric widget-panel
  layout (one tall left pane, two stacked right panes), distinct from
  `view-grid`'s uniform 3×3 dots and `virtual-desktops`' four equal tiles.
  This name was not in the manager's five explicitly listed names but was in
  `verify_shell_icon_coverage.cmake` (`DashboardApplet.qml`); added per "not
  a selected subset."

`tools/validate_qinda_icon_theme.py`'s `REQUIRED_NAMES` is no longer a
hand-copied set: `_required_names_from_coverage_cmake()` parses
`tests/shell/verify_shell_icon_coverage.cmake` at import time with the same
regex above, with a floor assertion (`< 40` names raises) so a parser
regression can't silently shrink the contract. No CMake or runtime file was
touched; only the already-added `mkdocs.yml` icon-theme nav entry from the
prior candidate remains outside `data/icons/**`/`tools/**`/the wiki page.

## 3. Worker record correction

Commit `8a1d8685` fixes `ops/team/workers/mineral-icons-claude.md` only:
replaced invented mid-session timestamps (`18:45`, `19:05`, `19:20`, `19:35`,
`19:42`) with the actual logged process start you gave,
`2026-09-05T19:21:27-06:00`, plus one current-time reconciliation entry
(`2026-09-05T19:52:54-06:00`) — I have no independently logged per-event
timestamps for the original session, so I did not invent replacements for
them either. Provider is now `Claude Code CLI`; model `claude-sonnet-5`;
reasoning states plainly that the actual per-turn level isn't independently
verifiable from this side rather than asserting `medium`. No other worker's
record was touched.

## 4. Updated visual review

Rendered a labeled sheet of all wifi variants plus the other new/changed
names (both renderings) to `.cache/icon-review/wifi-and-repair-sheet.png`
inside this worktree (gitignored, not committed, not `/tmp`) using
`.cache/icon-review/render_sheet.py` + `rsvg-convert`, and inspected it
directly. Wifi levels none→excellent show a strictly increasing arc count;
`-off` (slash, no arcs) and `-disconnected` (dashed single arc) read as
distinct from each other and from the signal levels;
`network-bluetooth-activated`/`bluetooth` both render identically to
`network-bluetooth-active` as intended for a declared alias; `view-refresh`
reads distinctly from `system-reboot`; `dashboard-show` reads distinctly
from `view-grid`/`virtual-desktops`.

## Commands run and exit statuses

- `python3 tools/generate_qinda_icon_theme.py` → exit 0.
- `python3 tools/validate_qinda_icon_theme.py` → exit 0, `validated 286
  QindaQt SVG assets: 131 canonical icons, 12 aliases, 7 semantic groups`.
- `find data/icons/QindaQt -name '*.svg' -print0 | xargs -0 -n1 xmllint
  --noout` → exit 0 on all files.
- `/tmp/qindaqt-mkdocs-venv/bin/mkdocs build --strict` → exit 0, "Documentation
  built in 2.29 seconds", no warnings (unchanged since the prior candidate;
  no wiki edits in this repair).
- `python3 tools/validate-docs` → exit 0, "Validated 170 Markdown documents
  and mkdocs.yml navigation."
- `python3 tools/check-source-shape` → the three touched modules
  (`qinda_icon_catalog_devices.py` 148, `qinda_icon_catalog_actions.py` 192,
  `validate_qinda_icon_theme.py` 158 non-blank lines) do not appear in its
  warning or largest-files output.
- Regression proof: reverted `_wifi_level()` to the original bug in this
  worktree, ran `python3 tools/generate_qinda_icon_theme.py &&
  python3 tools/validate_qinda_icon_theme.py` → exit 1, correctly failed
  with `stray non-whitespace tail content on
  <{http://www.w3.org/2000/svg}circle>: 'M27 42a7 7 0 0 1 10 0' -- a drawing
  helper call is likely unwrapped raw path data`; then restored the fix and
  reran → exit 0 again.

## Remaining bounded caveats

- Same caveats as the prior handoff stand: no full C++ build, no `ctest
  --test-dir build/dev -R 'docs|links'` (out of this task's scope).
- The WiFi alpha-mask check depends on `rsvg-convert` and `Pillow` being on
  the validator's `PATH`/`sys.path`; both were confirmed present in this
  environment. It is a verification-time dependency of the validator script
  only, not a new runtime or build dependency of the theme itself.
- `network-wireless-offline` from the prior candidate is unchanged and still
  present alongside the new `-off`/`-disconnected`; it isn't required by the
  coverage contract but removing an already-accepted, working icon was out
  of scope for a focused repair, so I left it as-is.

My worker record (`ops/team/workers/mineral-icons-claude.md`) is `idle`.
Requesting re-review of `d8c246be` (catalog/tool fix) with `8a1d8685`
(record-only) alongside it.
