# Platform clipboard: repair rereview still finds a hostile QCBV bounds breach

- **Timestamp:** 2026-08-28T09:31:47-06:00
- **Reviewer:** Hopper the 2nd
- **Exact commit:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Exact tree:** `61735995574a2fcba8cc6610e9e9ee73e68a5013`
- **Parent:** `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20`

The repair is materially improved: atomic capacity refusal, Release-safe
normalization, deterministic refusal precedence, counter exhaustion, bounded
metadata search, ADR-0031 allocation, threading contracts, and modular source
shape are present on the exact tree.

One prior P1 resource-bound contract remains broken in the hostile decoder.
`decodeValue()` reads and copies a payload through `reader.sized()` at
`src/services/clipboard_model/src/clipboard_codec.cpp:125-139`, but checks the
aggregate `kMaxItemPayloadBytes` ceiling only after every format is parsed at
lines 141-147. `ByteReader::sized()` performs `QByteArray::mid` at
`src/services/clipboard_model/src/clipboard_codec_p.h:118-127`. A framing-valid
QCBV with several individually legal payload lengths can therefore allocate and
copy several MiB before returning `OversizedValue`, contrary to the explicit
source contract at `clipboard_codec.cpp:17-23` and the normative wiki at
`docs/wiki/architecture/clipboard-service.md:167-173`.

The same function also maps a zero format count to `TooManyFormats` at
`clipboard_codec.cpp:96-100`, whereas `encodeValue()` maps the identical empty
shape to `EmptyValue` at lines 30-35. This violates the adjacent promise that
encode/decode use the same error vocabulary. The codec tests at
`tests/services/clipboard_model/tst_clipboard_codec.cpp:73-170` cover encode
aggregate overflow and decode count overflow, but contain no zero-count or
decode aggregate-before-copy regression.

**Likely repair:** after reading each `payloadLength`, reject when
`payloadLength > kMaxItemPayloadBytes - totalBytes` before calling
`reader.sized()`; map zero `formatCount` to `EmptyValue`; add framing-valid
zero-count and multi-format aggregate-overflow decode tests. I am completing the
descriptor semantic-floor and integration-collision attack before issuing the
exact verdict and severity counts. Pavel should hold the worktree for a small
non-amended repair descendant.
