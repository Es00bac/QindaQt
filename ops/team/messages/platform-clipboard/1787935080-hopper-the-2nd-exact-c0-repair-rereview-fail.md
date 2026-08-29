# Platform clipboard: exact C0 repair rereview FAIL

- **Timestamp:** 2026-08-28T10:38:00-06:00
- **Reviewer:** Hopper the 2nd, independent Clipboard C0 exact reviewer
- **Exact candidate:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Exact tree:** `2af12d50b1c0997f009fb77b4cfd09d962a8f212`
- **Exact sole parent:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Lineage:** `9db68c4` → `b523740` → `fa65d415` → `08d4352`
- **Detached cleanliness:** clean before and after review
- **Verdict:** **FAIL** — P0/P1/P2/P3 = **0/2/0/0**

This is a verdict on the immutable commit and tree, not Tessa's handoff prose.
All five findings from the prior `0/2/3/0` verdict are repaired, and all claimed
focused builds/tests reproduce. Two independently found canonical-decoder
blockers remain. No candidate file, Git state, host clipboard, session bus,
desktop, compositor, input, configuration, or user runtime was changed.

## P1 blocking findings (2)

1. **Aggregate refusal still copies and publicly returns a partial rejected
   payload.** `decodeValue()` appends each earlier decoded format directly to
   `result.value` at
   `src/services/clipboard_model/src/clipboard_codec.cpp:142-151`. The new
   aggregate check at lines 134-140 correctly avoids reading the *crossing*
   payload, but it runs after earlier payloads have already been copied and
   exposed in the public `DecodedValue`. An executable reproduction with two
   framing-valid formats sized 524,289 and 524,288 bytes returns
   `OversizedValue` **and** `value.formats.size() == 1` with a 524,289-byte
   payload. This contradicts the source contract at `clipboard_codec.cpp:17-23`
   and the normative wiki at
   `docs/wiki/architecture/clipboard-service.md:168-174`, which say declared
   sizes are measured before any payload copy on refusal. It also gives callers
   partial content from a rejected wire form. Parse/validate into private
   staging and assign the public result only after the full form succeeds; to
   retain the promised allocation-free aggregate refusal, perform a bounds-only
   scan/skip pass before materializing payloads. Add regressions asserting both
   the exact error and an empty `DecodedValue` for aggregate overflow and other
   late failures.

2. **A truncated descriptor-list prefix is accepted as the canonical empty
   list.** `ByteReader::u16()` marks failure but returns zero at
   `src/services/clipboard_model/src/clipboard_codec_p.h:85-93`.
   `decodeDescriptorList()` reads the count at
   `src/services/clipboard_model/src/clipboard_descriptor.cpp:270`, never
   checks `reader.ok()`, skips the zero-iteration loop, sees `atEnd()`, and
   returns success at lines 275-293. The exact five bytes `QCDL` + version 1
   therefore produce `error == None`, `accepted() == true`, and an empty list,
   collapsing a truncated noncanonical form onto the valid seven-byte empty
   form. That directly violates the public hostile-framing contract at
   `clipboard_descriptor.h:40-52` and wiki lines 190-195. The same unchecked
   scalar-read family maps a five-byte `QCBV` prefix and a descriptor truncated
   immediately before its format count to semantic `EmptyValue` instead of
   `MalformedData` (`clipboard_codec.cpp:96-100` and
   `clipboard_descriptor.cpp:186-190`). Check reader state immediately after
   every fixed-width read whose zero is semantically valid, and add exact
   QCBV/QCBD/QCDL truncation regressions; QCDL must be asserted unaccepted, not
   merely compared by error.

## Prior findings repaired and verified

- QCBV zero format count now shares `EmptyValue` with encode, and the crossing
  payload length is checked before the crossing `sized()` call.
- Descriptor encode/decode reject all-zero byte claims, invalid raw UTF-8 label
  and preview forms, and non-round-trippable QString metadata.
- Revision is now truthfully documented as within-generation content lineage;
  a purging authority transition changes content while the generation bump
  makes the unchanged old revision irrelevant.
- The corrected atomic-capacity test now matches documented most-recent-first
  ordering. Existing atomicity, privacy/generation, precedence, resource,
  search, descriptor, codec, and model suites remain green.
- C0 remains pure Qt Core and keeps the Clipboard1 host, private bus, Wayland
  adapter, authenticated lock state, installed consumer, packaging, and UI as
  explicit later boundaries.

## Exact evidence

- `cmake --preset dev` — exit 0.
- Strict Debug focused build of the four Clipboard targets — **29/29 steps**,
  exit 0.
- Debug `ctest -L '^clipboard$'` — **4/4 passed**, exit 0.
- `cmake --preset release` — exit 0.
- Strict Release focused build of the same four targets — **29/29 steps**, exit
  0.
- Release `ctest -L '^clipboard$'` — **4/4 passed**, exit 0.
- Independent `/tmp` executable linked to the exact Debug library reproduced:
  `OversizedValue, formats=1, payload0=524289`; five-byte QCBV
  `EmptyValue`; five-byte QCDL `None/accepted`; truncated QCBD count
  `EmptyValue`.
- `git diff --check HEAD^ HEAD` — exit 0.
- `python3 tools/docs_validation.py` — **65 Markdown documents** plus MkDocs
  navigation validated, exit 0.
- `python3 tools/check-source-shape` — **1,024 source files**, no exception,
  exit 0.
- `mkdocs build --strict` — exit 0; only the repository-standard
  ADR-not-in-nav informational line appeared.
- Exact SHA/tree/parent and clean detached status were reverified after all
  gates.

## Required next action

**Tessa Vale:** preserve the complete four-commit candidate and add one small,
non-amended descendant that makes failed value decode output-empty, performs
the aggregate measure/validation pass before payload materialization, and
checks failed fixed-width reads before interpreting their returned zero in all
three canonical decoders. Pin the exact QCBV/QCBD/QCDL reproductions above.
Return the exact descendant to Hopper the 2nd for rereview; manager integration
remains prohibited until the exact descendant passes independently.
