# Early legacy reader refusal and method monitor — bounded verdict

- Reviewer: qinda_icon_brand_audit, source-only, independent of root operation.
- Exact product base: 5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c.
- Root-provided actual outcomes: GNOME-only Unavailable7 after0.836s; KWallet-only InvalidInput1 after0.080s. Both catalog absent and zero encrypted collection files.
- Reviewer performed no actual bus/provider/monitor/import/prompt/secret calls, tests, builds, GPU activity or product edits. Only low-priority source reads and own records.

## Operation verdict

REJECT the proposed destination-only BecomeMonitor scope. Logging only member/signature/sender prevents value output, but the monitor receives the complete matched D-Bus messages in memory. Destination-only matching can include unrelated applications' writeEntry/writePassword/SetSecret calls containing secret values.

ACCEPT a narrower bounded root operation: a disposable private monitor connection with bus match rules restricted to type=method_call, the exact pinned source unique owner, and an explicit read/acquisition member allowlist. Wallet members: wallets, openAsync, folderList, entryList, entryType, readEntry, close. Secret Service members as actually needed: Get, GetAll, Unlock, OpenSession, GetSecrets, ReadAlias, Close, and Prompt for the existing acquisition path if offered. Exact interface constraints where known further restrict each member. Do not match returns, errors, signals, writes or all traffic as a fallback.

Callback must defensively recheck method-call type and accepted destination/member, then emit only get_member/get_signature/get_sender. Do not access body, object path, arguments, to_blob, message representation, dumps or error-reply details. Incoming allowed requests still contain labels/path arguments in memory; no claim that monitoring transfers headers only. Output is method metadata, not user data or secret payload. Use private0600 evidence, finite cap/lifetime and owned connection cleanup; denied monitor permission ends this diagnostic without widening scope.

One root scratch-source retry may then give the ordered acquisition call sequence. Maintain genuine native admission, unchanged signed importer, reviewed fresh diagnostic destination and EOF transport. Full sequence is necessary: KWallet RAII close after a parse failure and Secret Service session Close after a failure can be the final observed RPC. The last non-cleanup acquisition member is a candidate boundary, not proof of the exact reply defect. Unrelated callers remain distinguishable by sender; never attribute another application's calls to the importer.

This is an operation-shape verdict; no concrete monitor script/hash was submitted, reviewed or executed.

## Early KWallet code1 source sites

legacy_kwallet.cpp8–9 expects wallets/folderList/entryList return as. legacy_wire.cpp124–133 verifies actual pinned sender, METHOD_RETURN and exact expected reply signature; signature mismatch at130 produces InvalidInput. Codec verifies each array element type/UTF-8 and rejects duplicate strings at legacy_wire_codec.cpp47–53. Public introspection confirms declared contracts but cannot prove the actual reply envelope.

openAsync request uses wallet string, int64 window0, application string, boolean session at legacy_kwallet.cpp15–20; reply must i. A negative returned transaction is Unavailable7, not1. waitSignal requires exact walletAsyncOpened ii at legacy_wire.cpp144; malformed matching signal produces1. Timeout is Cancelled4, not1. Valid completion with negative handle is Cancelled4. No static evidence establishes a transaction parser bug.

After a handle, each entryType reply must i and values outside1..3 produce1 at legacy_kwallet.cpp52–54. readEntry must ay and strict array-byte shape is checked. Root's declared metadata types/names/duplicates constrain but do not replace the actual importer caller's replies.

Reader acquires two complete passes before planning: legacy_reader.cpp30–35. Snapshot differences produce Conflict, not1. Planner can reject invalid collection/item source IDs, duplicate identities, KWallet type/folder conditions, empty batch or aliases. Root's reported no xdg-desktop-portal folder excludes the exact64-byte portal-derived special case for those sampled snapshots.

Important localisation: openLegacyReader at legacy_reader.cpp46–48 catches every constructor failure and throws runtime_error; CLI catch at import_main.cpp101 maps it to7. KWallet1 therefore cannot directly be Wire constructor binding/PID rejection. It is an acquisition/parse/planner rejection or earlier CLI invalid PID argument. 0.080s alone does not establish which RPC was reached.

GNOME7 can arise in constructor/setup, a missing/error RPC reply, generic allocation or other unavailable acquisition path. No source message by itself proves an RLIMIT_MEMLOCK cause. The presence or absence of provider method calls is useful evidence; do not infer source data invalidity from7.

## Source bindings

| Path | SHA256 |
| --- | --- |
| src/services/keyring/import/legacy_kwallet.cpp | 0c61c73d93b11b90aedb407760f565bd770485b2825aa15079ce90ba6f87ef3e |
| src/services/keyring/import/legacy_wire.cpp | 591583e9d4980f5e9c04b9332bd32c49b874598e87427ca8f11283e63e1ad2b1 |
| src/services/keyring/import/legacy_wire_codec.cpp | 6e073592f48c3eb5b1b3d13ec78dca077db666cdc5121334945e41d842eb78af |
| src/services/keyring/import/legacy_reader.cpp | a45c57ffb06519356bb03c55dbe0a065c4a4cb67720a73792fb347914891b2ae |
| src/services/keyring/import/legacy_snapshot_plan.cpp | a4fb60ec6a446291e30a4bc2086754cde6d5a07d7a3df6c15a4af9e0d6d1bac9 |
| src/services/keyring/app/import_main.cpp | 003d05ecdbad021fef05fc5a40c587d807338b4022eb749bb1e8c7a5707bc5d2 |

No production defect is proven from static review. Smallest next gate is the bounded allowlisted method-header trace during root's genuine scratch diagnostic, then identify the failing acquisition boundary from that receipt. Do not rebuild the desktop or introduce a diagnostic framework.
