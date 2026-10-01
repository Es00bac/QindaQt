# Combined runtime gate and native portal integration

- Compiled source: `32bf9f77522591093bb2954cc33e08daee70f530`; exact fork690 production stage
- Added actual fixture targets: qindaqt_powerdevil_import_tests, qindaqt_resident_lock_service_tests, qindaqt_qt_native_lock_request_tests; strict build exit0
- Gate union: exact accepted runtime22, privacy9 and Sleep7 regexes deduplicate to33 CTest rows
- Result:32 passed,1 failed,57.32s; no suite-pass claim
- Failure: qindaqt.kwayland-dpms-controller, admittedConnectionTracksCapabilityRemovalAndRestore line67, final restore expected5 set requests but observed4; revoked-lineage scenario passed
- Logs: ignored qinda sibling pf-combined-20261001-build-runtime-fixtures.log and pf-combined-20261001-runtime-privacy-sleep-tests.log; exact regex/name list in build/combined/manager-runtime-privacy-sleep-gates.json
- Next action: isolated DPMS owner repairs/assertion-preserving proof, different-worker review, integrate and rerun affected union plus native/plugin/key-store scenarios

Independent PF18 candidate684ca passed24/24 real frontend/native/routing/staged/adjacent rows and is merged at4b946ad1; record-only review integrated01d9e50b. These later product changes require a new combined build before reusing runtime binaries for that tree. No installed service or package transition occurred.
