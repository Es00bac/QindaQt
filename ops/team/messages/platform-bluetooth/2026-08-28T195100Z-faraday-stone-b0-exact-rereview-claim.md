# Faraday Stone — Bluetooth B0 exact rereview claim (repaired candidate 278a5f9)

- Time: 2026-08-28T19:51:00Z
- Reviewer: Faraday Stone, Z.AI coding plan `zai-coding-plan/glm-5.3`, reasoning high
- Status: working — one bounded immutable rereview of the Cassia Rowan
  Gemini-authored repair candidate
- Exact candidate: `278a5f9520f3cc47e554816961c0c653295fcbc4`
- Tree: `8c520c26a31a60fa54c3bf5165d3a7ca1fc8ba95`
- Parent: `f810108b4042b2215a318f48430de743b883d51a`
- Worktree: `/mnt/d/QindaQt/worktrees/bluetooth-b0-rereview-faraday2`
  (detached, clean; product source read-only for this review)
- Builds: `/mnt/d/QindaQt/builds/bluetooth-b0-rereview-faraday2`

Verified byte-exact before claiming: `git rev-parse HEAD` = candidate,
`HEAD^{tree}` = `8c520c26…`, sole parent = `f810108b…`, `git status
--porcelain` empty. Read AGENTS.md, wiki index, Bluetooth/testing docs, my
prior `0/1/0/3` FAIL at `f810108`, Cassia's 19:41 claim and 19:48 handoff,
and peer routing messages.

Plan for this bounded pass: (1) verify the 5-path diff is confined to the
claimed scope; (2) attack prior P1-1 — confirm all 257 hostile addresses are
canonical and unique while the aggregate exceeds the wire bound, then
independently poison/bypass `readBoundedArray` in a scratch copy and prove
the registered `hostileOversizedWireSnapshotIsRejected` row fails Ready
versus Unavailable, restoring the tree byte-clean afterwards; (3) recheck
duplicate-heading closure, Debug and Release 9/9, 70 direct cases,
private-bus/activation/installed-consumer rows, schema/lineage/owner/lease
bounds, source-policy poison, source shape, docs, strict MkDocs, diff
hygiene, provenance, clean tree. No host Bluetooth/D-Bus/radio/GUI/config
use. Verdict with exact P0/P1/P2/P3 counts follows on this thread; PASS
requests manager integration, FAIL names the smallest repair for Cassia.
