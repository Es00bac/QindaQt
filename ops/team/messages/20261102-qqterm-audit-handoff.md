# QQ_Term baseline audit — verified repairs, parity remains open

- Time: 2026-11-02T17:18:56+00:00
- User outcome: audit against mainstream desktop terminals and repair confirmed application gaps.
- Product source: QindaQt_Apps `44088c4bc08471585f78f18b391a0d2d7a4790f3`;
  packaging/tag commit `e8e998a`, `v0.3.1`; integrated on master and qinda hub.
- Overlay: `dadd1aa`, exact `=gui-apps/qqterm-0.3.1`, source pin 44088c4,
  toolkit subslot 18 retained; integrated on master and qinda hub.
- Rejected experiment: QindaTK `0a165c57`, branch `codex/terminal-text-20261102`
  preserved on the hub only. QindaTK main and installed toolkit unchanged.

## Changed boundaries

Application: TerminalText helper (bounded Unicode/regex search, explicit URL
handling, quoted drops), TerminalAccessibility (read-only Qt text interface),
TerminalInput (IME/key proxy), terminal/search UI, window-local zoom, test fixture,
new interaction tests and explicit failing engine acceptance probe. Existing
profile schema and desktop executable/desktop-entry/bus identity are preserved.

Documentation: apps/terminal, the obsolete Settings1 terminal-profile claim,
task/handoff summary and this worker/message record. Source docs/terminal-parity.md
contains the reference matrix and acceptance evidence. No features.json step advanced.

## Verification

- Direct CMake/Ninja build: exit 0, actual configured MAKEOPTS `-j24 -l24` retained.
- App CTest: 4/4 groups, exit 0, 12.63 seconds. QtTest counts 10 CLI, 12 app,
  9 interaction; Python D-Bus launch fixture passed.
- Harness follow-up: both GUI groups individually own private D-Bus sessions;
  2/2 groups, exit 0, 10.01 seconds, same 12+9 QtTest counts.
- Actual QDropEvent reaches a private PTY as quoted arguments without Enter.
  Multiline Cancel sends no input; URL tests intercept dispatch.
- Qt accessibility: Unicode text, selection, character geometry and reading
  cursor pass. Native Orca/AT-SPI remains unqualified.
- Screenshots inspected: terminal, narrow search and paste review; existing
  profile/palette GUI fixtures also pass. No production credentials or live PTYs used.
- Separate `terminal_parity_probe`: exit 1, reports resize text cropping and
  physical newlines in wrapped copies against installed QindaTK r19/libvterm 0.3.3.
- Rejected engine experiment: lifecycle 3 pass, modes/history 25 pass, but
  resize fixture aborts with exit 134 on a long line crossing history; wide
  reflow also adds a space. Do not merge or package the experiment.
- Ebuild manifest: exit 0 (git-r3, no distfile). pkgcheck: exit 0 with inherited
  local maintainer-email metadata, Python eclass and visible-VCS findings.
- Strict MkDocs: exit 0. Repository checker: 583 Markdown documents and navigation,
  exit 0; the obsolete terminal anchor was corrected.

## Delivery and next action

Read-only Portage resolution: exit 0, exactly one QQ_Term upgrade and zero
downloads. Installed QQ_Term remains 0.3.0-r1. No dependency upgrades are selected.

The exact update is prepared, not installed. `qinda-sync packages --prepare-only`
stopped at `sudo -n` with `a password is required`; no credential dialog or
installation was attempted afterward. User authentication is required before
Portage can build/sign/install this one package. Use `sudo -v`, then
`python3 tools/qinda-sync packages '=gui-apps/qqterm-0.3.1'` in QindaGentoo.
Do not substitute the default entire-delivery update.

Priority remains the logical-line/history/resize engine repair. It must pass the
counterexamples and preserve genuine spaces, wide glyphs, cursor state and
primary/alternate buffers before a parity claim. OSC 8, extended scrollback,
configurable shortcuts/selection and real assistive-technology evidence are
also still outstanding. No delegated model processes were launched.
