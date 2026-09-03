---
author: Ida Rhodes
date: 2026-09-02T20:46:28-06:00
topic: shell-clipboard-applet
type: claim
base_commit: 74da46345c7a5094d45c756ad8b23ca87591fcd3
branch: worker/clipboard-applet-c1
worktree: /home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1
status: working
---

# Clipboard applet C1 claim (salvage-and-finish)

Claiming QQ-004.15 Clipboard applet C1 as Ida Rhodes (Moonshot Kimi,
`kimi-code/k3`, reasoning high), exact base `74da463` (current `main`),
branch `worker/clipboard-applet-c1`, build root
`/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1`.

Salvage first: Orion Vale's preserved chain (`5e48b5c` implementation,
`69b3edc` Hopper-blocker repair, `610e81f` unreviewed Liskov repair2 WIP)
is being cherry-picked onto current main with conflict resolution. Its
sound design is preserved; the two Tarski P1 blockers (real pointer
delivery to Pin/Delete; synchronous search-reply attribution) and the
remaining P2s get explicit repair plus hostile regressions before handoff.

Two salvage edits are deliberately dropped because they touch paths this
lane does not own: `src/services/clipboard_model/CMakeLists.txt` (PIC +
duplicate install; main already exports the model headers) and
`src/applets/CMakeLists.txt` (manifest component install). The packaged
manifest component instead lands in `src/shell/CMakeLists.txt`, mirroring
the Power applet's `PowerAppletRuntime` component seam.

This lane adds what the salvage never claimed: the
`BuiltinAppletRegistry::firstParty()` entry, policy registration,
manifest/catalog/resolver test rows, and the additive wiki/mkdocs records.
`src/shell/runtime/**` and `src/shell/qml/**` stay untouched (another lane
composes the production shell after integration);
`src/services/clipboard_model/**` stays read-only.

Gates before handoff: strict Debug+Release builds, focused
`^qindaqt\.clipboard-applet-` selector plus adjacent
applets/applet-runtime rows in both profiles, boundary poison with
negative control, installed-package/RPATH row, `tools/validate-docs`,
strict MkDocs, `tools/check-source-shape`, `git diff --check`, JSON
validation. Then: independent exact review, then manager integration.
