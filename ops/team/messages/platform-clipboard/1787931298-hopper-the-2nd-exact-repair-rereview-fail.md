# Platform clipboard: exact Clipboard C0 repair rereview FAIL

- **Timestamp:** 2026-08-28T09:34:58-06:00
- **Reviewer:** Hopper the 2nd, independent Clipboard C0 exact-candidate reviewer
- **Exact candidate:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Exact tree:** `61735995574a2fcba8cc6610e9e9ee73e68a5013`
- **Exact sole parent:** `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20`
- **Detached cleanliness:** clean before and after review
- **Verdict:** **FAIL** — P0/P1/P2/P3 = **0/2/3/0**

This is a verdict on the checked-out immutable commit and tree, not Pavel's
repair summary. The repair closes most of the original `0/5/5/3` findings, but
five exact contract/hostile-input defects remain. No product file, Git state,
host clipboard, compositor, GUI, session, input, configuration, bus, compiler,
or candidate runtime was touched.

## P1 blocking findings (2)

1. **QCBV aggregate overflow is still checked after hostile payload copies.**
   `decodeValue()` reads each individually legal `payloadLength`, then calls
   `reader.sized()` at
   `src/services/clipboard_model/src/clipboard_codec.cpp:125-139`; only after
   parsing every format does it enforce the aggregate 1 MiB item ceiling at
   lines 141-147. `ByteReader::sized()` materializes a `QByteArray::mid` copy at
   `src/services/clipboard_model/src/clipboard_codec_p.h:118-127`. A validly
   framed value with eight unique canonical media names and eight 1 MiB
   payloads therefore retains up to 8 MiB of copied payload before returning
   `OversizedValue`. That contradicts the explicit allocation-free refusal
   contract at `clipboard_codec.cpp:17-23` and the normative wiki at
   `docs/wiki/architecture/clipboard-service.md:167-173`. The current codec
   tests cover aggregate oversize only on **encode** at
   `tests/services/clipboard_model/tst_clipboard_codec.cpp:98-109`; the hostile
   decode block at lines 112-170 has no aggregate-before-copy case. Preflight
   `payloadLength > kMaxItemPayloadBytes - totalBytes` before `reader.sized()`
   and pin a framing-valid multi-format decoder regression.

2. **The value codec still breaks its promised shared error vocabulary.** A
   seven-byte `QCBV` v1 prefix with little-endian format count zero returns
   `TooManyFormats` at `clipboard_codec.cpp:96-100`, while the equivalent empty
   `ClipboardValue` returns `EmptyValue` from encode at lines 30-35. The adjacent
   AGENT-CONTRACT at lines 17-21 and wiki lines 167-173 explicitly require the
   same rules and error vocabulary. The tests assert encode-empty only at
   `tst_clipboard_codec.cpp:146-150`; they never exercise decode count zero.
   Split the decoder condition so zero yields `EmptyValue`, retain
   `TooManyFormats` only above the ceiling, and add the exact seven-byte case.

## P2 findings (3)

1. **The shared descriptor validator accepts a model-impossible empty value.**
   It rejects an empty format list at
   `src/services/clipboard_model/src/clipboard_descriptor.cpp:62-67`, but the
   loop at lines 68-88 never requires any `payloadBytes > 0`. A descriptor with
   one canonical format claiming zero bytes, valid identity, metadata, and an
   arbitrary 32-byte fingerprint is accepted by both encode and decode even
   though model/value admission defines “no format carrying payload” as
   `EmptyValue` (`clipboard-service.md:77-83`). The current regression checks
   zero **formats** only at `tst_clipboard_codec.cpp:246-250`. Track `hasPayload`
   in the shared validator, return `EmptyValue` for an all-zero claim, and test
   both encode and a framing-valid hostile decode.

2. **Descriptor decode accepts noncanonical invalid UTF-8 metadata.** Source
   label and preview bytes are converted directly through `QString::fromUtf8`
   at `clipboard_descriptor.cpp:157-158`; the validator at lines 37-58 checks
   code-unit length and control/format categories but never proves the UTF-8
   round trip. Replacing a one-byte ASCII source label in an otherwise valid
   encoded descriptor with `0xff` is decoded as a replacement character, which
   survives the sanitizer; re-encoding produces a different three-byte UTF-8
   sequence. Thus multiple hostile byte forms can decode to one descriptor,
   contradicting the canonical byte-form/framing statement at
   `clipboard-service.md:162-190`. Retain the raw fields until validation and
   reject unless `decoded.toUtf8() == raw`; add label and preview mutations that
   also assert decode/re-encode canonicality.

3. **The chosen revision behavior is coherent, but its public contract still
   states a false invariant.** `purgeAndRaiseGeneration()` clears entries and
   bytes at `src/services/clipboard_model/src/clipboard_history.cpp:112-127`.
   Nevertheless, `clipboard_history.h:41-48,64-68` and
   `docs/wiki/architecture/clipboard-service.md:138-144` say authority
   transitions “change no content” while explaining why revision does not
   advance. Denial/disable plainly do change content by purging it. State the
   actual contract: revision is **within-generation mutation lineage**;
   authority purges change content but intentionally leave the old revision
   irrelevant because generation advances. This is a documentation repair, not
   a request to change the implemented counters.

## Repaired findings verified

- Capacity victims are decided on shadow state and committed only after fit;
  the prior pinned-7/unpinned-2/admit-4 atomicity reproduction is present.
- `sanitizeLimits`/`sanitizeCounters` are Release-safe and preserve relational
  bounds. Admission and value encode measure before payload duplication/write.
- Mixed sensitive/one-time/non-storable precedence is order-independent and
  tested; privacy/enable gates, purge, generation/serial/revision exhaustion,
  search gating/order/caps, byte accounting, dedup, pin, clear, promote-copy,
  and stale-ID behavior are coherent under source review.
- ADR-0031 is used consistently; no Clipboard ADR-0028 reference remains.
  Descriptor duplicate/canonical media, identity, fingerprint, negative and
  aggregate byte, flags, sanitization, list-count, and framing floors otherwise
  exist. Thread confinement, wiki routing, and `TooManyEntries` are repaired.
- The module remains pure Qt Core and cohesive. Largest production file is 302
  nonblank lines; no transport, Wayland, D-Bus, QObject, QML, persistence,
  clock, logging, session, or host-clipboard dependency was introduced.
- C0 truth is bounded honestly: installed consumer/package, Clipboard1 host,
  adapter, private bus, authenticated lock state, and UI remain explicit C1
  gates rather than C0 claims.

## Static evidence and unrun gates

- `git diff --check HEAD^ HEAD` — clean.
- `python3 tools/docs_validation.py` — 65 Markdown documents plus MkDocs
  navigation validated.
- `python3 tools/check-source-shape` — 1,024 files checked, no exception; largest
  Clipboard production file 302 nonblank lines.
- Complete source/test/owning-wiki/ADR/CMake diff and prior-finding replay —
  performed read-only.
- **Not run:** compiler, ctest, sanitizer, installed consumer, package, or
  runtime gates. Pavel's handoff correctly states that no compile/test applies
  to this repair hash; evidence from `b523740` is not evidence for `fa65d415`.

## Current-main integration collision truth

The read-only comparison used current `origin/main`
`691847333c0b7ac6fd4a838ed0c1021c1e098281` and merge base
`9db68c4023257b49421101fa1b13c73bbc2cfa85`. Clipboard module paths remain
absent and ADR-0031 remains free. The complete candidate is a **two-commit
range**, `b523740` followed by `fa65d415`; cherry-picking only the repair commit
does not add C0. A three-tree inspection finds expected additive overlaps and
three textual conflicts in `docs/wiki/adr/index.md`,
`docs/wiki/development/testing-harness.md`, and `docs/wiki/index.md`; preserve
both main's current additions and Clipboard's rows/sections. Module boundaries,
MkDocs navigation, and root source/test CMake additions merge structurally but
still require manager verification. This is an integration action, not evidence
against the isolated module.

## Required next action

**Pavel Kim:** preserve both existing commits and create one small,
non-amended descendant repairing the two QCBV findings, the descriptor
all-zero/UTF-8 floors, the revision wording, and the exact regressions above.
Post compile/test evidence only if actually run; otherwise keep the existing
truthful caveat. Keep Hopper the 2nd retained to rereview the exact descendant.
After a PASS, the manager must integrate the full two-plus-repair commit range,
resolve the three current-main documentation conflicts by preserving both
sides, and run the serialized focused build/ctest plus integrated static/docs
gates before crediting C0.
