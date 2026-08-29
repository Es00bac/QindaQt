# Compile findings and manager efficiency correction (Veda Park)

Material fact for the Network N0 candidate: the preserved network sources had
never compiled before the crash. Five in-place repairs (no reset, no
reimplementation) were required under the repository's strict warning flags:

1. src/services/network_protocol/src/network_redaction.cpp:64 —
   `QStringView::trimmed()` returns `QStringView`, not `QString`.
2. src/services/network_protocol/src/network_validation.cpp:245 — the
   capability mask `capabilities.toInt() & ~(1<<4)` rejected every snapshot
   carrying any capability at all (every valid fixture). Repaired to the
   intended unknown-bit check `(capabilities & ~allKnownCapabilityBits) != 0`,
   which matches the validation tests' accept/reject intent.
3. src/services/network_model/include/.../network_model.h — missing include of
   network_intent_policy.h (IntentVerdict relied on a transitive include).
4. src/services/network_client/src/network_client.cpp — `Model::ApplyResult`
   must be `Model::NetworkModel::ApplyResult` (nested type).
5. Focused test repairs: scan-lease test CMake include dir; zero-argument
   clock helper; `~Capabilities(Capability::X)` flag negation; qsizetype
   conversions; QVERIFY/QTRY (bare `return;` on failure) removed from the
   non-void makeClient helper; nodiscard results handled.

Process: the manager interrupted my broad full-tree Debug build and I record
that as a deliberate manager efficiency correction, not a product failure. The
/dev configure remains valid, already-built artifacts stay, and from here I
build only the Network protocol/model/client libraries, their focused test
executables, and the boundary/installed-consumer rows; the
combined-integration worker owns the one broad full-tree build after
acceptance.

Wiki pages added this session: docs/wiki/architecture/network-service.md,
docs/wiki/reference/network1-v1.md, mkdocs.yml nav, wiki index links.
