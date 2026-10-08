# Audio installed-QML SOURCE ACCEPT

- Time: 2026-10-08T13:21:00Z
- Reviewer: GPT Audio density reviewer
- Exact source candidate: 72bf0c28359ffe876742ec71bc1cfed567a2b417
- Base: 9a48f76440a6dc95b67f5f47ba59eca77643986f
- Paired old-production/new-regression control: a8ff66fe5c3a6ace4ddb16f2b0f11dfcd21b5f54
- Disposition: SOURCE ACCEPT; no new compiler/native/package/installed qualification.

Production changes only `src/apps/settings/audio/CMakeLists.txt`. The Qt-owned query returns the absolute source file list and matching relative deployment paths; the ZIP install preserves each directory/name instead of flattening a separate inventory. All 23 QML declarations, including 11 console inputs, are covered. The recipe, Audio model, service, QML presentation, public API, gain/persistence policy and dependencies are unchanged. This fixes the inherited missing disk closure; it does not alter the rejected immutable R19 artifact.

Owning test changes are `tests/apps/settings/audio/CMakeLists.txt`, new `check_installed_qml.cmake` and new `installed_qml_probe.cpp`; primary documentation changes only `docs/wiki/apps/audio-settings.md`. Peer own claims/board are the other candidate paths. No shared test registry or another module implementation is edited. Production CMake has 120 nonblank lines, new probe105 and driver93; the additive owning registry428 remains a build registry. The install/test responsibilities are cohesive and no new production decomposition threshold is crossed.

The staged consumer imports Audio/Tokens/Controls from the actual owning install rules plus static Shell Icons; public external dependency links exclude system QindaQt. It replaces the engine import list and clears inherited import/library/plugin environment paths. No source/build/system Audio QML can rescue a withheld installed file. The existing model is metadata-only with no transport activation. Public `ensureTokenFacade`/shipped `ThemeLoader`/GUI-thread token publication/`IconRuntime` precede QML; the full AudioPage owns the QindaQtTheme bridge and natural inner section layout. The 1280×720 outer-page Devices/Mixer captures exercise representative construction and visible common faders; the separate accepted compact-density proof is retained.

The harness reads all 23 qmldir declarations, verifies deployed source hashes, checks component readiness through compiled and forced-disk paths, requires full page construction and rejects positive undefined-token/script warnings. The intended complete fixed run has 30 invocations; this is a prospective count, not execution evidence. Missing-module exit3 is an explicit missing-qmldir precheck before QQml import. Each expected disk-poison exit1 still requires the actual missing path/type cause in the native raw. The first unexpected CMake FATAL_ERROR stops execution and may leave its private stage poisoned; restoration must be reported from actual evidence, not assumed on failure or interruption.

Independent read-only checks completed:

- All five indexed changed source/doc blobs equal exact72bf and clean author source bytes, including recorded SHA256/nonblank counts. All23 declaration/source blob SHA256 entries independently verified; console11, unique/file-presence23.
- Whole old-control `src` equals base9a48 and whole old-control `tests` equals fixed72bf: each actual Git diff exit0. Exact fixed src/tests equal clean metadata-only81344dace2a348ea81c6c97f52332dfb79df66b9: each exit0.
- Full base→72bf Git diff whitespace check exit0; applicable ancestry instructions are rootAGENTS only. Changed paths/source boundary and public bootstrap declarations read from exact Git objects; existing Icons/Controls query install precedents and local public Qt query implementation independently read.
- Executed only the pure `python3 -B tools/validate-docs` checker on the source-equal author tree: exit0,534 Markdown documents/navigation,0.456s. Owner preserved strictMkDocs raw ends with successful build7.99s; owner receipt reports exit0. Reviewer did not rerun MkDocs, configure, compile, native QML or CTest.
- Independent hash/link output and copies of owner source index/closure/strict-doc log are preserved in reviewer ignored `.cache/audio-install-source-independent-20261008` on qinda. Source index SHA2562e3b079b1c8d434884e4bf24ec86daa4181c9b9165aaf3fcf27c13bcc567f87a; declaration indexa37a1c945181a174cf834285531f630bec9dded4ba5457aff8feaf74e7913a5f.

Requested next action: root may grant the already isolated old-first/fixed native cohort to Platform, with strict resource ownership and preserved causal raw/captures. Native, a fresh corrected release/archive/package, installed/physical and whole-ED gates remain separate. No previously accepted density/archive/crypto audit is reopened. Reviewer is available for the exact resulting raw/images or a repaired source boundary.

Own full staged diff check includes the live-board transition and both new replies: exit0. Final committed candidate check and preserved reviewer commit are reported with the handoff.
