# Exact removable-media protocol handoff

- Date: 2026-10-07T18:37:02+00:00
- Worker: Everyday Files Sol
- Candidate: `f88974a5f90f447f0026ef6bd2d8d966a4ed93f2`
- Exact base: `0d15023cc31f7b46e31a8fa3447c80aa7624505c`
- Branch: `worker/everyday-media-protocol-20261007`
- Canonical active worktree: `everyday-media-protocol-20261007` (retained)
- Preservation: attached directly to qinda bare hub; no origin/GitHub push.
- Requested next action: same independent reviewer inspects/tests exact candidate before integration. ED04 remains open.

## Scope and paths

New `src/services/removable_media_protocol/**`: owning typed public values/limits/errors, pure structural validators, bounded canonical little-endian codec. New `tests/services/removable_media_protocol/**`: split valid/hostile/validation tests, durable standalone CMake entry point, independent staged consumer. One additive registry line each in `src/CMakeLists.txt` and `tests/CMakeLists.txt` beside NetworkProtocol, manager approved; root CMake unchanged. Documentation: `docs/wiki/reference/removable-media-protocol-v1.md`, `docs/wiki/architecture/module-boundaries.md`, `docs/wiki/adr/0350-share-removable-media-with-file-consumers.md`, `docs/wiki/apps/removable-media.md`, one `mkdocs.yml` reference nav line. Self-authored worker record/new timestamped thread replies only beyond owning module/docs.

No client/exporter/connection/UDisks/persistence/launch/policy/presentation/new dependency. No hardware/runtime/ED04 completion claim. Largest production file129 nonblank lines. Source-only public error metadata; bounded readers check lengths/counts before allocation, temporary destination publishes atomically, malformed UTF8/source UTF16 refuses without replacement, literal BOM/U+FFFD preserved. Repeated sibling drive id allowed; duplicate volume id/attachment handle refuses; retired pending-removal lineage allowed.

## Exact gates and proofs

All commands ran in canonical worktree on qinda. Direct builds used actual configured Portage MAKEOPTS `-j24 -l24`. Read-only prebuild load7.66/16.53/15.90,20GiB available; unrelated Games renderer compiler left untouched.

- `cmake -S build/media-protocol-harness -B build/media-protocol-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`; build `cmake --build build/media-protocol-debug -- $(portageq envvar MAKEOPTS)`; `ctest --test-dir build/media-protocol-debug -V`: exit0,3/3,62 Qt checks (9codec36hostile17validation including init/cleanup),0fail/skip. Same configure/build/ctest for `media-protocol-release` with Release: exit0,3/3 and62checks. Initial handwritten empty golden vector had one excess zero byte; fixed independent field literals, production unchanged.
- Durable owning harness `cmake -S tests/services/removable_media_protocol/standalone -B build/media-protocol-public-harness -G Ninja -DCMAKE_BUILD_TYPE=Debug`, same exact build MAKEOPTS and verbose CTest: exit0,3/3,62checks.
- `cmake --install build/media-protocol-release --prefix "$PWD/build/media-protocol-stage" --component QindaQtRemovableMediaProtocol`: exit0; archive+4publicheaders staged only. Configure installed_consumer with `QINDAQT_STAGE_INCLUDE_DIR=$PWD/build/media-protocol-stage/include` and `QINDAQT_MEDIA_PROTOCOL_LIBRARY=$PWD/build/media-protocol-stage/lib64/libqindaqt_removable_media_protocol.a`; build exact MAKEOPTS; CTest exit0,1/1. Ninja command audit proves only staged public include/archive plus Qt host headers/libs. Temporarily absent staged media_codec.h causes compiler missing-header failure; restored build/CTest exit0. No source/build API fallback, no host install.
- Normal `cmake -S . -B build/media-protocol-registry-usr -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec -DKDE_INSTALL_DATADIR=share -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_PRODUCTION_SHELL=ON`: exit0; module and3CTest registrations generated. No full compile/runtime. Earlier default layout failed frozen installed capture paths; shellOFF configure failed existing missing ShellSurface test target. Correct compatible options resolve both without unrelated edits.
- `mkdocs build --strict`, `python3 tools/docs_validation.py`, `git diff --check`: exit0,516 Markdown documents/nav validated.

Proofs (ignored build files retained): `build/media-protocol-{debug,release}-{configure,build,ctest}.log`; `build/media-protocol-public-harness-*.log`; `build/media-protocol-stage.log`; `build/media-protocol-consumer-{commands,configure,build,ctest,missing-header}.log`; `build/media-protocol-registry-usr-configure.log` and generated protocol build/test registrations; `build/media-protocol-{mkdocs,docs-validation}.log`.

## Remaining boundary and status

Transport/read-only owner exporter/client readiness/replay fencing/graphical startup/runtime admission/authoritative media operations/sidebar/chooser await separate reviewed slices. Codec validates structure only and establishes no owner trust or device authority. Existing unrelated QindaTK/GlobalMenu import warnings remain visible in normal configure; final generation exits0. No compiler/private-bus lease remains. Available for exact reviewer repair; no new work until manager followup.
