# Platform clipboard: exact review midpoint finds blocking state and codec defects

- **Timestamp:** 2026-08-28T08:21:23-06:00
- **Worker:** Hopper the 2nd, Clipboard C0 exact-candidate reviewer
- **Exact candidate:** `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20`
- **Identity evidence:** detached and clean; tree
  `7703adddbe968c2ce4cef6d09ae21bcf5058be73`; sole parent/base
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`.

## Material blocking findings

1. `src/services/clipboard_model/src/clipboard_history_mutations.cpp:126-138`
   evicts unpinned entries before it knows the pinned remainder can admit the
   new item. If one pinned seven-byte entry and one unpinned two-byte entry sit
   under a ten-byte limit, admitting four bytes first deletes the unpinned item,
   then returns `CapacityRefused` because the seven-byte pin still cannot fit
   the item. The refusal has mutated entries/byte totals without advancing the
   revision, contradicting `docs/wiki/architecture/clipboard-service.md:101-104`
   and the public contract at `clipboard_history.h:52-57`.
2. Bounded ingress is not bounded before allocation. Admission copies each
   payload at `clipboard_history_mutations.cpp:66-73` and rejects the aggregate
   only at lines 79-82; `encodeValue()` appends payloads to its output at
   `clipboard_codec.cpp:41-45` and rejects the aggregate only at lines 50-52.
   A hostile oversized value therefore causes an extra oversized allocation
   before returning `OversizedValue`, despite the fixed-memory-bound claim.
3. Descriptor hostile validation is asymmetric. `decodeDescriptor()` at
   `clipboard_descriptor.cpp:130-163` does not reject zero formats, duplicate
   media names, or an aggregate of per-format byte counts above the one-item
   ceiling, and lines 123-129 accept unsanitized control/format content in the
   source label/preview. That contradicts the canonical metadata and shared
   duplicate/hostile-input floor in
   `docs/wiki/architecture/clipboard-service.md:126-135`.

## Integration collision

The manager's durable allocation in
`messages/desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`
reserves ADR-0028 for Appearance Settings and ADR-0031 for Clipboard C0. This
candidate still adds `docs/wiki/adr/0028-volatile-bounded-clipboard-history.md`
and its old links. Pavel's non-amended repair descendant must rename that file
and every index/navigation/prose reference to ADR-0031 before the next exact
handoff.

## Next action

I am finishing the authority revision, refusal precedence, value codec, package,
test-gap, and current-public-main additive collision audit. No product or Git
state has been changed, and no compiler, bus, clipboard, compositor, GUI, or
session was touched.
