# Pre-handover application-data verification — existing idempotent route

- Exact integrated source:912729b5abbda9058fbd3bde327792f67d89ff82.
- Reviewer:qinda_icon_brand_audit; own isolated worker/pf-keyring-driver-r3-review-20261003.
- Root reports actual laptop import SUCCESS0,3 sealed collections/12 items, original stores retained. Qinda full import outcome is not inferred.
- Source-only procedure review: no imports/providers/prompts/credentials/UI/installation/tests/builds/GPU or product edits.

## Selected minimum existing path

Use one exact full-source idempotent production import retry against the SAME sealed native storage root, with existing normal native destination unlock prompts. Retain old providers and original stores, quiesce application writes to the selected sources, keep the native resident/writer absent, and retain genuine compositor/session/PID/socket/NativeLock admission. Use the same accepted r3 production importer and default prompt; no diagnostic password FD, no new app/provider proxy or fixture.

The existing wrapper90fa refuses catalogExists, so actual retry requires only a narrow explicit, separately reviewed operational retry switch permitting that already sealed destination. Do not globally remove the first-import guard or enable retries against arbitrary existing roots. Do not rebuild product code; docs keyring-import.md135–145/174–180 already define exact idempotent retry and destination password authentication.

For laptop the required receipt is EXIT0 with0 collections added,0 items added,3 collections unchanged, matching the original3-collection/12-item sealed receipt. CLI99 does not print itemsUnchanged, but the transaction's authenticated item-count/every-item equality proves those existing3 collections still contain their matching full contents. Qinda must use its own actual initial full-import collection count once known. EXIT0 alone is insufficient: the importer only verifies collections present in its current plan, so an omitted source collection or newly added collection must not be described as exact verification of the previous complete snapshot.

## Why this verifies data preservation

collection_import_transaction.cpp12–17 compares item ID, attributes, label, content type, creator, created/modified timestamps, secret length and exact secret bytes using CRYPTO_memcmp. Existing collection provenance, label and dates must match at79–81. Each destination collection is authenticated with its actual password at82–88, remains locked afterward through RAII89, and must have authenticated index with exactly the planned item count90–96. Every planned item must exist and compare equal; missing or extra items refuse with Conflict.

The transaction counts unchanged records97; existing stores are never saved. New storage creation is only in the else branch100–108. With no pending additions and unchanged aliases it returns at134 before catalog publication137. Existing repository opening still performs its standard orphan recovery; this is the documented owning store transaction, not a claim of globally read-only filesystem behavior.

The unchanged existing test_complete_sources_exact_bytes_restart_idempotence in tests/services/keyring/test_legacy_import.py64–70 performs the same exact retry, requires unchanged collections, and checks encrypted file bytes and catalog equality. Its private fixture is existing acceptance coverage, not a reviewer test or actual user-data receipt.

## Scope and limits

This is full-record application-data preservation verification before the provider/PAM switch. It does not prove an ordinary application's live authentication against the native Secret Service, provider name ownership, fresh native login or PAM auto-unlock. Those truthful runtime claims remain separate after actual handover; the old provider/package/data can remain available for rollback until their actual retirement gate.

There is no stronger already runnable pre-handover ordinary-client gate on the same host bus while legacy owns org.freedesktop.secrets. Do not invent a substitute identity, bypass admission, private proxy or new framework to claim one. A generic synthetic service test does not verify the user's actual data.

If sources changed since sealing, cancellation/password mismatch/mutation/conflict must retain the real receipt and leave old providers in place. Do not reinterpret additions or a partial matching plan as exact idempotent preservation. The normative cutover plan's prerequisite already says actual acquisition+sealed import and application-data compatibility precede source provider unmerge/native PAM initialization. Its historical actualState still records pre-import facts at912 and must not be treated as current completion evidence; root's real sealed receipts are the current facts.

Checks: owning transaction, CLI output, normative import/daemon docs, existing idempotence fixture and cutover prerequisite source inspected. No operations/tests by reviewer. Stop available; root prepares the narrow reviewed operational retry switch and executes the existing genuine prompt path.
