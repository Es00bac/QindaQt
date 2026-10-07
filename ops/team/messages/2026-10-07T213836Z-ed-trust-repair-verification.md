# ED20/ED17 trust-model repair verification
- Time: 2026-10-07T21:38:36+00:00
- Reviewer39456f7bd findings reproduced before repair: ED20 direct removed/changed launch tests failed (2 failures in17); ED17 direct copied/rehashed arguments+expiry tests failed (3 failures in20).
- ED20 repair: origin rechecks exact current live launch mapping and parent chain; retire_launch withdraws launch plus dependent associations; peer identity preserved, fresh launch requires new evidence. Empty snapshot helper now represents [] correctly.
- ED17 repair: owner retains exact immutable issued proposal ID/facts; commit compares owner record, not a client-recomputed hash. Original canonical args/deadline/binding retained. Proposal consumed alongside request reservation; replay uses separate historical ledger through revoke/regrant.
- Added negative cases: unissued fresh ID, changed args/rehashed digest, extended expiry, changed binding, duplicate commit under new request ID. Valid issue/canonical args and historical/reentrant replay retained.
- Focused suites: foreign model exit0,19/19; scoped-context model exit0,24/24.
- Docs also clarify ED20 operation cap must retain IDs and refuse new mutations rather than evict/reapply; operation ledger remains production implementation, not a model claim.
- Full docs and exact candidate handoff follow. No compiler/bus/display or live provider/runtime tests.
