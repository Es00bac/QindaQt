# Tessa Rowan — Controls S2 exact-review midpoint

Timestamp: 2026-08-27T21:38:27-06:00

Candidate: `10996f146ff78f69a6f1019933d812d1475faf85`, tree
`ed48f540b36f8d2d7f1f865d4493d02c74f9daf0`.

## Independently closed surfaces

- The detached exact tree remains clean; the only `shell/` diff path is the
  owned documentation page `docs/wiki/shell/controls.md`. No production shell,
  Settings1, service, LayerShellQt, Kirigami, AppShell, or theme data path was
  changed, and no broad pass is claimed.
- All 25 committed PNG hashes exactly match Cora's durable hash inventory.
  Their dimensions are exact: 15 rows at 420/720/1080 by 840 for 100%, five at
  900x1050 for 125%, and five at 1080x1260 for 150%. I independently inspected
  five newly assembled contact sheets; every theme/profile shows complete
  StateCard/DegradedNotice text, error, unavailable, busy, ordinary, checked,
  form, switch, and slider states without clipping or overlap.
- Exact CMake registration expands to behavior + 25 unique isolated rows +
  policy + PSS + installed import. The wrapper accepts only the scale's exact
  row set and requires one matching tagged QtTest pass. The preserved Release
  registry/cost evidence names all 29 once. The staged backing library has the
  direct Tokens dependency and `RUNPATH [$ORIGIN/../Tokens]`; the plugin finds
  its backing library from `$ORIGIN`.
- Independent static gates pass: `git diff --check`; 818-file source-shape with
  zero allowlist skips and largest changed file 496 non-blank lines; and
  `tools/validate-docs` over 46 Markdown documents plus navigation. Public
  ownership/lifetime/GUI-thread/error/compatibility prose, dependency direction,
  supported `available` behavior, FormRow association, total hostile preview
  validation, RTL geometry, focus, and source modularity are coherent.
- Preserved Release PSS output contains all three paired samples and exactly
  supports the reported medians/delta/null-threshold. This is measurement, not
  a memory-budget pass, as the wiki and handoff correctly state.

## P2 — StateCard's documented dynamic announcement contract is incomplete and order-dependent

`docs/wiki/shell/controls.md:54` promises that every post-construction semantic
transition uses the Qt announcement API. The public announcement tuple is
status + title + message (`src/controls/qml/StateCard.qml:42-68`), but only
`onStatusChanged` invokes it (`:71`). A Warning/Error card whose `message` or
`title` changes while its status stays the same produces no announcement.
Worse, a normal view-model update that publishes status before title/message
announces synchronously with the old title/message, and the later content
updates never correct it. Users of assistive technology can therefore receive
silence or stale error detail through the documented public path.

The test named `stateCardAnnouncesDynamicSemanticTransitions`
(`tests/controls/tst_controls_behavior.cpp:324-378`) changes only status while
keeping title/message constant, so it exactly mirrors and misses both defects.
It also relies on public writable implementation properties
`accessibilityReady` and `accessibilityRevision`
(`StateCard.qml:25-26`), leaving a consumer-visible switch that can suppress
all announcements and making test bookkeeping accidental 1.0 API.

Requested repair: make readiness/revision private implementation state (the
signal count already proves revisions), schedule/coalesce status/title/message
changes after construction, and announce exactly one complete latest tuple for
a multi-property update. Add deterministic regressions for (1) a same-status
Warning/Error message update and (2) status followed by new title/message in
one event turn, requiring no stale/intermediate tuple and one final tuple with
the new content and correct politeness. Document the coalescing/lifetime rule.
This should be a new non-amended descendant of the rejected candidate.

The installed-module P2 in `1787888115` remains independently blocking. I am
finishing the remaining evidence/comment-policy pass without using Mira's
compiler lane, then will post one consolidated exact FAIL verdict for Cora's
single repair round.
