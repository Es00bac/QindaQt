# Bundled apps on stock Qt 6 — program close-out

- Worker: kimi-bundled-apps (Moonshot Kimi CLI)
- Status: handed off — program complete
- Date: 2026-09-10T13:10:00Z

## Outcome

All four bundled applications are first-class stock Qt 6 apps per ADR-0116.
Commits on `main` (each verified at its own tree):

- `40bd202c` — ADR-0116 + re-scoped policy docs
- `737ebf27`, `34013338` — Calendar: stabilized/committed, stock Controls,
  real event editing + details pane, 12/12 rows, wiki page, ebuild component
- `337edf3a`, `5d54c653` — File Manager: stock Controls, clipboard, DnD,
  properties, recursive search; 31/31 rows
- `504ff587`, `942b13e3` — Text Editor: de-chrome, printing, crash-recovery
  autosave; 27/27 rows
- `b8cfa02a`, `92cd4575` — Terminal: de-chrome, opt-in session restore,
  OSC-8 pinned unsupported with evidence; 24/24 rows
- `dc992d63` — font-bootstrap gate re-scoped; Terminal main() decomposed

## Gates at close-out

Full Debug build green under -Werror; focused app rows all green (counts
above); `mkdocs build --strict`, `docs_validation.py` (221 docs),
`check-source-shape` (no program findings) pass. `docs/HANDOFF.md` and
`docs/TASK_LIST.md` updated in the same integration.

## Bounded caveats

- Five `qindaqt.settings-*-installed-route` rows fail environmentally: the
  September 8/9 system-wide Portage installs placed QindaQt SettingsApp
  modules in `/usr/lib64/qt6/qml`, so the relocation-poison stage borrows the
  host module (QML_IMPORT_TRACE evidence in HANDOFF). Predates this program;
  routed to the release/settings lane.
- Display-dependent rows need `WAYLAND_DISPLAY=qindaqt-0`.
- Ebuilds pin pre-program commits; re-pin >= `dc992d63` for the next package.
- Deferred per plan: CalDAV, per-volume Trash/mounts/SMB, editor spell-check,
  terminal OSC-8 (pinned unsupported), physical-printer qualification.

Requested next action: user re-pins the desktop ebuild and rebuilds through
Portage; release lane picks up the installed-route harness contamination.
