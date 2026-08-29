# Platform clipboard: second exact C0 descendant rereview PASS

- **Timestamp:** 2026-08-28T10:51:09-06:00
- **Reviewer:** Hopper the 2nd, independent Clipboard C0 exact reviewer
- **Exact candidate:** `aad0ff2d6b35d2223a61f5528964614cba03fcc9`
- **Exact tree:** `34e0da73d62b19b2cc594083957c2574ba601f87`
- **Exact sole parent:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Complete lineage:** `9db68c4` → `b523740` → `fa65d415` → `08d4352` →
  `aad0ff2`
- **Detached cleanliness:** clean before and after review
- **Verdict:** **PASS** — P0/P1/P2/P3 = **0/0/0/0**

This verdict is based on the immutable checkout, direct source/test review, and
independent executable reproductions. No candidate source, commit, branch,
host clipboard, session bus, desktop, compositor, input, configuration, or
user runtime was changed.

## Blocking findings closed

- `scanEncodedValue()` now performs a complete payload-free first pass. It
  checks every fixed-width read, canonical media and duplicates, per-format and
  cumulative payload bounds, payload extents, nonempty content, and trailing
  bytes before materialization. `ByteReader::skip()` advances only after a
  bounds check and never constructs a payload `QByteArray`.
- The materialization pass stages a private `ClipboardValue` and publishes it
  only after the complete already-validated form succeeds. The exact hostile
  524,289 + 524,288 byte aggregate now returns `OversizedValue` with **zero**
  formats and no partial public payload.
- QCBV, QCBD, and QCDL now check reader state before interpreting failed count
  reads as semantic zero. Five-byte QCBV/QCDL prefixes and QCBD cut before its
  format count return `MalformedData`; the canonical seven-byte empty QCDL
  remains accepted.
- Descriptor-list decode stages entries until the entire list succeeds, so a
  later nested/trailing failure cannot expose an accepted prefix.
- The public headers and owning wiki truthfully state the two-pass and atomic-
  publication contracts. Focused regressions pin aggregate, duplicate,
  trailing, truncated-count, and no-partial-output behavior.

## Independent hostile-input evidence

A review-only `/tmp` executable linked against the exact Debug library replayed
the prior reproductions and iterated **every strict prefix** of a valid two-
format QCBV, valid QCBD, and valid two-entry QCDL:

- aggregate overflow: `OversizedValue`, `formats=0`, no payload;
- five-byte QCBV: `MalformedData`, unaccepted, empty value;
- five-byte QCDL: `MalformedData`, unaccepted, empty list;
- QCBD cut immediately before format count: `MalformedData`, unaccepted,
  empty descriptor;
- every shorter QCBV/QCBD/QCDL prefix: unaccepted with empty public output;
- aggregate prefix scan failures: **0**.

## Exact verification

- Exact clean detached SHA/tree/sole-parent identity verified before and after
  review.
- Strict Debug incremental focused build of all four Clipboard targets — exit
  0.
- Debug `ctest -L '^clipboard$'` — **4/4 passed**, exit 0.
- Strict Release incremental focused build of the same targets — exit 0.
- Release `ctest -L '^clipboard$'` — **4/4 passed**, exit 0.
- `git diff --check HEAD^ HEAD` — exit 0.
- `python3 tools/docs_validation.py` — **65 Markdown documents** and MkDocs
  navigation validated, exit 0.
- `python3 tools/check-source-shape` — **1,024 source files**, no exception,
  exit 0.
- `mkdocs build --strict` — exit 0; only the repository-standard ADR-not-in-nav
  informational line appeared.

## Bounded remaining boundary

This PASS qualifies the pure Qt Core C0 candidate only. Clipboard1 transport,
private bus/authentication, Wayland adaptation, lock-state authority,
installed-consumer/package evidence, and clipboard UI remain explicit C1
outcomes. No claim is made for those later slices.

## Requested action

The manager should integrate the **complete five-commit Clipboard candidate
lineage**, preserve current-main additions while resolving any shared docs/build
registries, rerun focused gates on the integrated tree, and only then advance
the live feature evidence. Hopper the 2nd is available for any exact integrated
regression review.
