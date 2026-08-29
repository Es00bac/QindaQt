# Platform clipboard: second exact C0 repair handoff `aad0ff2`

- **Timestamp:** 2026-08-28T10:47:28-06:00
- **Worker:** Tessa Vale
- **Exact commit:** `aad0ff2d6b35d2223a61f5528964614cba03fcc9`
- **Exact tree:** `34e0da73d62b19b2cc594083957c2574ba601f87`
- **Exact sole parent:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Complete lineage:** `9db68c4` → `b523740` → `fa65d415` → `08d4352` →
  `aad0ff2`
- **Branch/worktree:** `worker/clipboard-c0-repair-tessa` at
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0-repair-tessa`
- **Cleanliness:** clean after commit and exact post-commit gates
- **Requested action:** Hopper the 2nd immediate immutable rereview of exact
  `aad0ff2`; integration remains prohibited until PASS

## Exact blockers closed

1. QCBV decode performs a complete payload-free scan before materialization.
   The scan validates count/media/duplicates, every fixed-width read,
   per-format and cumulative sizes, payload extents, nonempty content, and
   trailing bytes using `ByteReader::skip()` for payloads. Only a fully valid
   form enters the copying pass.
2. The materialization pass stages a private `ClipboardValue` and assigns the
   public result only after all reads succeed. The exact 524,289 + 524,288 byte
   aggregate returns `OversizedValue` with zero formats; duplicate, trailing,
   truncated-count, and aggregate-truncated failures also expose empty values.
3. QCBV, QCBD, and QCDL check reader state immediately after their fixed-width
   count reads. Exact five-byte QCBV/QCDL forms and a valid QCBD cut immediately
   before format count return `MalformedData`; the valid seven-byte empty QCDL
   remains accepted.
4. Descriptor-list decode now also stages its entries until complete success,
   so a trailing or nested failure cannot expose an accepted prefix. Public
   header and owning wiki wording record both atomic-publication contracts.

## Changed paths

- `src/services/clipboard_model/src/clipboard_codec.cpp`
- `src/services/clipboard_model/src/clipboard_codec_p.h`
- `src/services/clipboard_model/src/clipboard_descriptor.cpp`
- `src/services/clipboard_model/include/qindaqt/services/clipboard_model/clipboard_codec.h`
- `src/services/clipboard_model/include/qindaqt/services/clipboard_model/clipboard_descriptor.h`
- `tests/services/clipboard_model/tst_clipboard_codec.cpp`
- `docs/wiki/architecture/clipboard-service.md`

## Exact evidence

- Strict Debug focused incremental build — exit 0.
- Debug `ctest -R '^qindaqt\\.clipboard-model-(media|history|history-lineage|codec)$'`
  — **4/4 passed**, repeated after exact commit.
- Strict Release focused incremental build — exit 0.
- Same Release selector — **4/4 passed**, repeated after exact commit.
- `git diff --check HEAD^ HEAD` — exit 0.
- `python3 tools/docs_validation.py` — **65 documents** and MkDocs navigation
  validated, exit 0.
- `python3 tools/check-source-shape` — **1,024 source files**, no exception,
  exit 0.
- `/tmp/qindaqt-docs-venv/bin/mkdocs build --strict` — exit 0; only the
  repository-standard ADR-not-in-nav informational line appeared.

## Bounded caveats

This is still pure Qt Core C0 evidence. Clipboard1 transport, private bus,
Wayland adaptation, authenticated lock state, installed-consumer packaging,
and UI remain C1. No host clipboard, session bus, desktop, compositor, input,
or configuration was contacted, and no integration/live feature state was
edited.
