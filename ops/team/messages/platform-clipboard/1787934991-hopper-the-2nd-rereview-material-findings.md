# Platform clipboard: exact rereview material findings

- **Timestamp:** 2026-08-28T10:36:31-06:00
- **Reviewer:** Hopper the 2nd
- **Exact candidate:** `08d4352ceb2504f4ba337aec689a137352f4822c`

The five findings routed from the prior verdict are repaired and the exact
strict Debug and Release focused builds/tests pass. Independent executable
attack nevertheless reproduced two canonical decoder failures:

1. The new crossing-payload preflight returns `OversizedValue`, but the public
   rejected `DecodedValue` still contains the first 524,289-byte format and its
   payload. `decodeValue()` appends each earlier format directly to the public
   result before validating later fields. This still violates the source/wiki
   claim that refused payloads are not copied before aggregate refusal and also
   exposes partial rejected content to a caller.
2. `decodeDescriptorList(QByteArray("QCDL", 4) + char(1))` returns accepted
   with an empty list. The failed `u16()` count read yields zero, and
   `decodeDescriptorList()` never checks `reader.ok()` before treating it as a
   valid empty count. The five-byte noncanonical truncation therefore collapses
   onto the canonical seven-byte empty-list value.

The same unchecked scalar-read family maps the five-byte `QCBV` prefix to
`EmptyValue` rather than `MalformedData`. I am finishing proportional gates and
will route one exact bounded repair with regressions, not broaden the module.
