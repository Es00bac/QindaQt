# Clipboard applet C1 exact rereview failed

- Author: Tarski Vale — Clipboard applet C1 independent exact rereviewer
- Provider/model/reasoning: Z.AI coding plan / `glm-5.3` / high
- Candidate: `69b3edc066739856424cdc7b99164693152697ff`
- Tree: `041666d53949eabc1d5eff5fe7c273800772ef55`
- Sole parent: `5e48b5cf4603cb3622237fb4d7d1ec197dcdd988`
- Verdict: **FAIL**, P0/P1/P2/P3 = `0/2/7/3`

## Blocking evidence

1. Real pointer clicks still never reach Pin or Delete. The full-row
   `MouseArea` in `src/shell/clipboard_applet/qml/ClipboardEntryRow.qml:183`
   remains above the action buttons. Reviewer-owned offscreen probes clicked
   the real button centers; `pinCalls=0` and `deleteCalls=0` in both Debug and
   Release, while row-body selection still works.
2. A synchronous search reply can be accepted for a superseded request.
   `src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:248` accepts
   a reply before the returned request id is registered. A hostile adapter
   flushed the queued `alpha` reply inside `requestSearch("gamma")`; the applet
   displayed `alpha stale secret` and discarded the real gamma reply.

Seven P2 findings remain: a synchronous clear-history pending-record leak,
unknown operation-completion payload injection, privacy phase/documentation
drift, unusable controls in degraded state, insufficient accessibility/pending
presentation evidence, missing reciprocal boundary/testing records, and
wall-clock promote ticks. Three P3 findings cover a Qt deprecation warning, a
`QApplication`/`qApp` poison-gate bypass, and QML token warnings.

## Evidence that did pass

- Exact identity and clean detached worktree verified before and after review.
- Debug and Release configure/build: exit 0, zero compiler warnings/errors.
- Applet/catalog tests: 10/10 per profile; Clipboard family: 12/12 Debug.
- Lock purge, pinned-first partition, async id fencing, installed packaging,
  explicit QML prerequisites, documentation validation, source-shape check,
  and strict MkDocs gates passed.

## Next action

Liskov Rowan owns a non-amended descendant that repairs both P1s and the seven
P2s, adds real-pointer and hostile synchronous-reply regressions, updates the
wiki/boundary/testing truth, and returns the exact commit for Tarski's rereview.
The rejected candidate remains preserved.
