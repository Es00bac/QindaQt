---
author: Liskov Rowan
date: 2026-08-28T17:20:00-06:00
topic: shell-clipboard-applet
type: claim
parent_commit: 69b3edc066739856424cdc7b99164693152697ff
candidate_tree: 041666d53949eabc1d5eff5fe7c273800772ef55
branch: worker/clipboard-applet-c1-orion
worktree: /mnt/d/QindaQt/worktrees/clipboard-applet-c1-orion
reviewer: Tarski Vale
status: working
---

# Clipboard applet C1 repair-2 claim (Tarski Vale FAIL 1787955600)

Resuming as Liskov Rowan (Z.AI coding plan glm-5.3, reasoning high). I own
one non-amended descendant of the rejected `69b3edc` repairing both P1s,
all seven P2s, and the safe P3s; parent work and history preserved.

Highest priority first: real pointer delivery to Pin/Delete (the full-row
MouseArea stacking above the buttons) with real-event click regressions, and
correct attribution of synchronous search replies so a hostile adapter
flushing a superseded reply inside `requestSearch` can never display stale
results. Then: synchronous operation-completion pending fencing (including
clear-history), rejection of unknown/duplicate completion payloads, privacy
phase/documentation truth, honest degraded-state control capability,
accessibility/pending presentation with tests, reciprocal boundary/testing
records, and controller-owned monotonic promote ticks replacing wall clock.

Straightforward P3s: `QApplication`/`qApp` poison-gate coverage; the Qt
deprecation and QML token warnings where reproducible and safely fixable.

Scope unchanged: Clipboard applet source/tests/manifest/primary docs plus
smallest additive registrations; no manager ledgers/providers, no unrelated
modules. Builds under `/mnt/d/QindaQt/builds/clipboard-applet-c1-liskov`,
focused Debug+Release tests plus hostile/mutation/package/docs/shape/MkDocs
gates. Will post midpoint/material findings/handoff with exact
commit/tree/parent, commands/counts/exits, caveats, and request Tarski
Vale's exact rereview.
