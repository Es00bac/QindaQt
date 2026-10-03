# Source-isolation EOF diagnostic — ACCEPT

- Reviewer: qinda_icon_brand_audit, independent of root operational-helper author.
- Exact product base: 5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c.
- Scope: read-only source and operational-script review. No execution, provider call, prompt, secret read, compiler, GPU, installation or product edit.
- Operational driver SHA256: 05bb16c92d68bba5909d9169275338470925925d923ddd405ab856843b0264be.
- EOF adapter SHA256: e43098d161020c71aa6489ac05673c001ed4fcb79e2dc57081efa1de186b735b.
- Scripts: root-owned build/install-final-20261002/credential-migration/run_laptop_import.py and diagnostic_empty_pipe.py. Local exact bytes independently read and hashed.

## Verdict and next gate

ACCEPT the corrected source-isolation operation, one GNOME-only invocation followed by one KWallet-only invocation after complete native-session teardown. This is an operational diagnostic, not a successful migration, installed desktop or provider cutover receipt.

The driver selects work/source-diagnostic-secret or work/source-diagnostic-kwallet, refuses a pre-existing diagnostic destination, creates it privately, and never selects the actual native destination in diagnostic mode. It removes exactly the opposite source-owner/PID/service arguments. Native compositor/session/PID/socket/lock admission and signed production importer remain unchanged. Both providers are still required by operational setup even though each importer invocation selects only one reader.

The installed resolved Python binary is exposed read-only at python_real and invoked at that same path inside the namespace. The adapter creates an anonymous pipe after bwrap, immediately closes its writer, verifies owned FIFO and pipe:[...] descriptor identity, makes the read end FD3 inheritable, and execs the unchanged importer. No bwrap preserve-fds assumption, password bytes, synthetic admission or partial migration is introduced.

With continuing genuine admission, nonempty valid acquisition and batch validation reach Passwords::take and immediate EOF returns Cancelled4. Admission loss instead returns OwnerLost6. Code4 is diagnostic cancellation; driver nonzero/operation_stopped is expected and must not be labelled imported or sealed. Code1 identifies a source/plan/validation refusal in that invocation. Other codes retain their actual meanings.

## Exact storage consequence

The original claim that EOF precedes all destination catalog/file publication was rejected. Catalog construction opens CollectionRepository before commit: collection_import_catalog.cpp9/23–25; collection_repository.cpp38–40/42–98. loadCatalog persists a bootstrap catalog when absent at98 and removes unlisted .qkr orphans when an existing catalog is present at93–94. Therefore real destination reuse is unsafe for this diagnostic; fresh per-source scratch is mandatory.

Fresh scratch can contain its directory, writer lock and empty bootstrap catalog. In collection_import_transaction.cpp75–108, password acquisition82 and error rejection83 precede CollectionStore::create104–106. Encrypted saving118–119 and imported catalog publication137 occur later. At first-password EOF no candidate or attempted encrypted file exists, so no acquired collection/item is created, saved or advertised. This is not a claim of zero scratch metadata side effects.

## Refusal localisation from previous actual receipt

Given root's original InvalidInput1 plus catalogExists:false at the exact stable destination, late commit validItem validation is excluded: import_main.cpp93 opens the repository/bootstrap before commit94. Constructor exceptions map Unavailable7 at101. Remaining locations are acquisition/planning or earlier CLI argument/PID parsing; this is conditional source inference, not an observed reader cause.

Secret Service real wire checks not covered by prior metadata include Unlock returned-path subset, OpenSession empty plain output/non-root session, GetSecrets exact signature, expected unique item paths, matching session and empty parameter bytes; legacy_secret_service.cpp40/43–72. Strict codec types, UTF-8 round-trip and duplicate/empty map keys are additional InvalidInput paths. Missing expected secrets returns Unavailable, not InvalidInput. Planner checks source/item identity uniqueness, bounded source IDs/folders, alias grammar/targets and nonempty collection batch; legacy_snapshot_plan.cpp31–130.

KWallet xdg-desktop-portal derived records require Stream type2 and exactly64 bytes at76–77. Root's latest metadata explicitly reports no such folder in both samples, excluding this candidate on those observations; it must not be asserted as the actual cause. Empty Secret Service contentType fails format.cpp93 at later commit validation, so it cannot explain the original no-bootstrap receipt under the stated stable-destination assumption.

## Source file bindings

| Source path | SHA256 |
| --- | --- |
| src/services/keyring/app/import_main.cpp | 003d05ecdbad021fef05fc5a40c587d807338b4022eb749bb1e8c7a5707bc5d2 |
| src/services/keyring/import/legacy_secret_service.cpp | 387eb8f66c20cc62e809a42d9b4db86b023e57b9a0c87364faf21c60c856239f |
| src/services/keyring/import/legacy_snapshot_plan.cpp | a4fb60ec6a446291e30a4bc2086754cde6d5a07d7a3df6c15a4af9e0d6d1bac9 |
| src/services/keyring/import/import_passwords.cpp | c99d6d081c8f5f08ab54d500d1f5cb34b876473bc4fa074c1958dbf88ee3e68d |
| src/services/keyring/import/collection_import_catalog.cpp | 70c2086d137db747b3350b750852eb9ca906a9aad051b5a0bc6bf698aa2a9589 |
| src/services/keyring/daemon/collection_repository.cpp | a4fa7a7271a927f39e78110ed571830f6112eb86b6a1ff9d5d85e4ca41684497 |
| src/services/keyring/daemon/collection_import_transaction.cpp | 599202c0027b7cdfd55c5f5b3c761b0f9de600ccc3c381803e0a2351e2a0bfbc |
| src/services/keyring/src/format.cpp | 0abf65747fc745e3694af92cfc18f34a87c450d07f19d108a025dc472163acc9 |

## Review history and limits

Initial real-root destination rejected for bootstrap/recovery side effects. ec330 scratch correction passed storage safety but was not executable with installed bwrap's unavailable preserve-fds option. 9cf inside-namespace adapter passed transaction safety; resolved interpreter path made explicit in final05bb. Earlier conditional accepts do not supersede this exact final binding.

No tests or diagnostic executions by reviewer. Source read/hash and owned-record diff check only. Root owns actual source acquisition and the resulting non-secret code/count/cleanup receipts. Stop available after immutable review handoff; no automatic product/runtime expansion.
