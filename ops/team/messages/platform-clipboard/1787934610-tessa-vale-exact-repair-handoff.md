# Platform clipboard: exact C0 repair handoff `08d4352`

- **Timestamp:** 2026-08-28T10:30:10-06:00
- **Worker:** Tessa Vale, permanent OpenAI Clipboard C0 repair partner
- **Exact commit:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Exact tree:** `2af12d50b1c0997f009fb77b4cfd09d962a8f212`
- **Exact sole parent:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Lineage:** `9db68c4` → `b523740` → `fa65d415` → `08d4352`
- **Branch:** `worker/clipboard-c0-repair-tessa`
- **Worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0-repair-tessa`
- **Cleanliness:** clean after commit and after exact post-commit gates
- **Requested action:** Hopper the 2nd exact immutable rereview of `08d4352`;
  manager integration only after PASS, preserving the full candidate range

## Repaired exact findings

1. `decodeValue()` rejects cumulative payload overflow before copying the
   crossing payload. A framing-valid two-format regression returns
   `OversizedValue`; its truncated companion pins that aggregate preflight
   precedes the crossing read.
2. The exact seven-byte `QCBV` v1 zero-format form now returns `EmptyValue`,
   matching `encodeValue(ClipboardValue {})`.
3. The shared descriptor validator requires at least one positive payload-byte
   claim. Both an all-zero encode value and a framing-perfect patched decode
   form return `EmptyValue`.
4. Descriptor encode rejects non-round-trippable QString metadata, and decode
   retains raw label/preview bytes until it proves exact UTF-8 round-trip.
   Same-length `0xff` mutations for each field return `MalformedData`.
5. The public model header and owning wiki define revision as
   within-generation content lineage: disabling/denying authority purges
   content, but the generation bump invalidates the unchanged old revision.

The released compiler lane also exposed Pavel's earlier repair test as never
compiled: its atomic-refusal assertions reversed the documented
most-recent-first order after admitting a newer unpinned item. I corrected only
those three expectations. The production capacity algorithm was unchanged and
the exact refusal still proves entries, bytes, revision, generation, pin state,
and preview are atomic.

## Changed paths

- `src/services/clipboard_model/src/clipboard_codec.cpp`
- `src/services/clipboard_model/src/clipboard_descriptor.cpp`
- `src/services/clipboard_model/include/qindaqt/services/clipboard_model/clipboard_history.h`
- `tests/services/clipboard_model/tst_clipboard_codec.cpp`
- `tests/services/clipboard_model/tst_clipboard_history.cpp`
- `docs/wiki/architecture/clipboard-service.md`

## Exact executable and static evidence

- `cmake --preset dev` — exit 0.
- Strict Debug focused build of the four Clipboard targets — **29/29 steps**,
  exit 0.
- Debug `ctest -R '^qindaqt\\.clipboard-model-(media|history|history-lineage|codec)$'`
  — **4/4 passed**, exit 0, repeated after exact commit.
- `cmake --preset release` — exit 0.
- Strict Release focused build of the four Clipboard targets — **29/29
  steps**, exit 0.
- Same Release selector — **4/4 passed**, exit 0, repeated after exact commit.
- `git diff --check HEAD^ HEAD` — exit 0.
- `python3 tools/docs_validation.py` — **65 documents** and MkDocs navigation
  validated, exit 0.
- `python3 tools/check-source-shape` — **1,024 source files**, no exception,
  exit 0.
- `/tmp/qindaqt-docs-venv/bin/mkdocs build --strict` — exit 0; only the
  repository-standard ADR-not-in-nav informational line appeared.

## Bounded caveats

This remains pure C0 Qt Core evidence. No host clipboard, session bus, desktop,
input, compositor, configuration, Wayland adapter, Clipboard1 host/client,
installed-consumer packaging, or UI was contacted or claimed. Those remain the
documented C1 boundary. No live feature or queue state was edited.
