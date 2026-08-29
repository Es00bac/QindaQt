# Platform clipboard: Clipboard C0 candidate handoff at exact commit `b523740`

- **Timestamp:** 2026-08-28T14:03:12Z
- **Worker:** Pavel Kim, Clipboard C0 service implementer
- **Exact candidate commit:** `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20`
- **Branch / worktree:** `worker/clipboard-c0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0`
- **Base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85` (exactly one commit on
  top; `git status` clean after commit)
- **Claim:** [1787922381-pavel-kim-claim.md](1787922381-pavel-kim-claim.md)
- **Midpoint:** [1787923446-pavel-kim-midpoint.md](1787923446-pavel-kim-midpoint.md)

## Requested next action

Different-worker exact-commit review of `b523740`; after acceptance,
manager integration with the additive registry/ADR rows preserved. This is a
static/model candidate: nothing in it is executable clipboard functionality
and no runtime or live-session claim is made.

## What landed (26 files, +2991)

- `src/services/clipboard_model/**` — `QindaQt::ClipboardModel`, Qt Core
  only, no QObject/IPC/Wayland/clock/persistence:
  - Bounded entry/value types with fixed protocol constants
    (`kMaxEntries`=64, `kMaxPinnedEntries`=8, `kMaxFormatsPerItem`=8,
    `kMaxMediaTypeLength`=127, `kMaxSourceLabelCodeUnits`=64,
    `kMaxPreviewCodeUnits`=96, `kMaxItemPayloadBytes`=1 MiB,
    `kMaxTotalPayloadBytes`=8 MiB); instances may narrow but never widen.
  - Canonical MIME metadata: lowercase/trim/shape canonicalization,
    allowlist classification (storable/non-storable/sensitive/one-time),
    refusal precedence sensitive → one-time → non-storable, sanitized
    source labels.
  - Volatile opt-in history: fail-closed start (disabled + privacy Denied),
    deterministic most-recent-first order, eviction of least-recent
    unpinned with pin sparing, fingerprint dedup that keeps identity and
    pin, bounded pinning, `UnpinnedOnly`/`All` clears (clear never raises
    the generation; no-op clear never bumps revision), capacity refusal
    that never mutates.
  - Privacy/opt-in/generation contracts: disable and privacy-denial purge
    and raise the generation by exactly one; every content mutation takes
    `expectedGeneration` and refuses `StaleGeneration` on mismatch;
    entry ids embed their generation; refusal order
    `HistoryDisabled` → `PrivacyDenied` → `StaleGeneration` → value
    validation → capacity is fixed and tested; payload bytes leave the
    model only via `promote()`.
  - Codecs: value (`QCBV` v1, bounded inline payloads) and descriptor
    entry/list (`QCBD`/`QCDL` v1, metadata-only with bounded preview),
    sharing one hostile-input decode floor (bounds-checked declared
    lengths, trailing bytes, unknown versions/flags, non-canonical media,
    duplicates).
- `tests/services/clipboard_model/**` — three focused suites
  (media/classification, history behavior, codec round-trip + hostile
  mutations) with obviously synthetic `"fixture …"` data only; no raw
  clipboard content anywhere.
- `docs/wiki/architecture/clipboard-service.md` (primary page, C0/C1 slice
  split and bounds table), `docs/wiki/adr/0028-volatile-bounded-clipboard-
  history.md`, additive rows in the ADR index, module-boundaries table and
  dependency bullets, and one mkdocs nav entry.
- Minimal additive registry edits only: one `add_subdirectory` line each in
  `src/CMakeLists.txt` and `tests/CMakeLists.txt`.

## ADR numbering note

Base carries ADRs through 0025; 0026 is taken by the virtual-desktop lane
and 0027 by the appshell lane on newer `origin/main`, so the clipboard ADR
is 0028. If integration renumbers, only the new file name, its index row,
and two intra-wiki links move.

## Exact verification evidence (commands and results)

All commands run in the worktree above; static/unit evidence only.

1. `cmake --preset dev` — configure clean (Qt 6.11.1, Ninja).
2. `cmake --build build/dev` — full dev-tree build, **1546/1546 steps, 0
   FAILED**; module compiles under strict warnings
   (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
   -Werror`).
3. `cmake --build build/dev` after the final header fix — `ninja: no work
   to do` (tree current), then `ctest --test-dir build/dev` — **238/238
   tests passed, 0 failed** (complete registry, includes the three new
   suites).
4. `ctest --test-dir build/dev -R "clipboard"` — **3/3 passed**:
   `qindaqt.clipboard-model-media`, `qindaqt.clipboard-model-history`,
   `qindaqt.clipboard-model-codec`.
5. Release configuration: `cmake --preset release`; focused build of
   `qindaqt_clipboard_model` + three test targets clean under the same
   strict-warning set; `ctest --test-dir build/release -R "clipboard"` —
   **3/3 passed**.
6. `mkdocs build --strict` (mkdocs 1.6.1 in an isolated venv) — clean; the
   ADR-not-in-nav INFO matches how every existing ADR is handled.
7. `python3 tools/docs_validation.py` — 65 documents validated, no issues.
8. `python3 tools/check-source-shape` — exit 0; largest new production file
   is 255 lines (largest new file overall is a 481-non-blank-line test
   suite, within guardrails).

## Known caveats (bounded)

- Three early test failures were in my own test expectations, not the
  module; the repaired expectations now pin dedup-vs-capacity interaction,
  the documented refusal order (privacy outranks staleness), and
  `isValidLimits` narrowing rules.
- The value codec carries bounded inline payloads by design (round-trip/
  test seam); the descriptor forms are the metadata-only snapshot basis.
  Large FD-based transfer semantics belong to the C1 transport slice.
- Not covered here, by design: private-bus behavior, Wayland/
  ext-data-control, lock-state provisioning, Settings1 opt-in wiring, UI,
  and any runtime qualification — all later Clipboard1 slices.
