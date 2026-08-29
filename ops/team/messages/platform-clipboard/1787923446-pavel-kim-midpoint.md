# Platform clipboard: Clipboard C0 model implemented, focused tests green

- **Timestamp:** 2026-08-28T13:24:06Z
- **Worker:** Pavel Kim, Clipboard C0 service implementer
- **Branch:** `worker/clipboard-c0` (base `9db68c4023257b49421101fa1b13c73bbc2cfa85`, not yet committed)
- **Claim:** [1787922381-pavel-kim-claim.md](1787922381-pavel-kim-claim.md)

## Material findings so far

- `src/services/clipboard_model/**` is implemented: bounded value/entry
  types with fixed protocol constants, canonical MIME canonicalization with
  an allowlist classification (storable / non-storable / sensitive /
  one-time), a volatile opt-in history model with deterministic
  eviction/dedup/pinning/clear, fail-closed privacy + opt-in gating,
  generation-fenced mutations, and value/descriptor codecs with a
  hostile-input decode floor.
- One design point worth recording for review: the descriptor carries the
  bounded text/plain preview excerpt by design (that is its presentation
  purpose); the descriptor guarantee is that no *complete* payload of any
  format is embedded and payloads leave the model only via `promote()`.
  The hostile test pins exactly that.
- Three initial test failures were all in my own test expectations (dedup
  intercepts a byte-pressure re-admit; privacy refusal outranks staleness in
  the documented gate order; a limits-narrowing helper violated
  `isValidLimits`). All repaired; the production module needed no behavioral
  change for them.

## Evidence (static/unit only — no runtime, bus, Wayland, or UI contact)

- `cmake --preset dev` — configure clean (Qt 6.11.1).
- `cmake --build build/dev --target qindaqt_clipboard_media_tests
  qindaqt_clipboard_history_tests qindaqt_clipboard_codec_tests` — clean
  under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`.
- `ctest --test-dir build/dev -R "clipboard"` — **3/3 passed**
  (`qindaqt.clipboard-model-media`, `-history`, `-codec`).

## Next

Wiki architecture page + ADR-0028, additive doc/CMake registry rows, full
dev-tree build, then a checkpoint commit and handoff.
