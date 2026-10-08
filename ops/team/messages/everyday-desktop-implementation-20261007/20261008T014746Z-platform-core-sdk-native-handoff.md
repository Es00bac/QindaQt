# Core SDK actual closure handoff

- Owner: Everyday Platform Sol, exposed collaboration runtime; no independent provider/process inference.
- Native-tested source: b6e3b3b331ff925571ac532d1bc2ea53462647a8; final descendant adds only own evidence/board.
- Exact old production base: a462c415a3678dc8dc96f15bd996abdceea20189.
- Own worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-core-sdk-20261008.
- Production change: one existing audio_console.h row added to src/services/audio_protocol/CMakeLists.txt FILE_SET.
- Full paths: owning test registration tests/services/audio_protocol/CMakeLists.txt, run_installed_audio_sdk.py, installed_sdk_fixture/CMakeLists.txt, installed_sdk_consumer/{CMakeLists.txt,main.cpp}; owning audio-service wiki SDK section; own board/replies.
- Lease: manager grant after Astra release, now all compiler/private fixture resources RELEASED. No Portage/buildpkg/install or live host service/session/device action.

## Actual commands and provenance

Old Audio modules were extracted by git archive from exact a462, then every resulting file compared byte-for-byte against git show of that commit (all matched). Same frozen gate uses this old production source; test source remains the corrected fixture. Actual commands:

    python3 tests/services/audio_protocol/run_installed_audio_sdk.py --source-root "$PWD/.cache/core-sdk-evidence/old-source" --output "$PWD/.cache/core-sdk-evidence/old" --native-makeopts "$(portageq envvar MAKEOPTS)"
    python3 tests/services/audio_protocol/run_installed_audio_sdk.py --source-root "$PWD" --output "$PWD/.cache/core-sdk-evidence/fixed" --native-makeopts "$(portageq envvar MAKEOPTS)"

Observed MAKEOPTS -j24 -l24, retained unchanged on every native cmake --build child. Portable default remains plain cmake build; no global job override. Production fixture actually compiles with -Wall -Wextra -Werror, not the repository's broader strict-warning configuration. Consumer compilecommands show C++20/Wall/Wextra/Werror, staged include then poison include, Qt include paths only; no production source include path. Consumer uses an inactive public QtAudioTransport on an absent named connection; it never calls start(). Child session/system bus addresses are nonexistent private paths. This linked lifetime does not start services or PipeWire.

## Exact old and fixed results

Oldroot .cache/core-sdk-evidence/old/audio-sdk-yvbbynik:
production configure/build/install and consumer configure exit0. 20 production Ninja actions completed; actual CMake FILE_SET installs8headers. Consumer build exits1 at named poison audio_console.h; driver exits1, preserving baseline failure. Host/source fallback cannot supply that header.

Fixedroot .cache/core-sdk-evidence/fixed/audio-sdk-b74tbrd5:
production configure/build/install, consumer configure/build/run, clean, restored build/run all exit0. 20 production Ninja actions completed; actual FILE_SET installs9headers, including console. Required header deletion produces consumer build exit1 at QINDAQT_REQUIRED_STAGED_HEADER_MISSING, then restoration builds/runs0. Complete driver exits0:10 child steps (9 expected exits0,1 required negative exit1). No skip or Qt test count claimed; this is an isolated native install/consumer gate, not a bus fixture.

Actual source/include fallback denial occurs through stage-first includes plus poison headers for known public QindaQt source headers before compiler host search. Installed exported targets provide staged static libraries and C++20 closure; no copying source header trees or modifying /usr/include.

## Durable raw evidence hashes

- Old results.json SHA25625894609850d158c794f455df11060b873952d3e911c7a4e1117be8fc177c18c.
- Old build-consumer.log SHA256518db4ba9a1d30ea1dee4c52141c1279ee101e0362a6bc3c03d6426a50d9c207.
- Old compile_commands.json SHA25602d776959c1844eeeb605fbc1c6a57492d2cc96271fa30302003ef3c2f6a04c1.
- Fixed results.json SHA2561fa550569756d98a903516a352b957c3970f06e2871e98fa40846652b5f7332c.
- Fixed build-production.log SHA256abc957f0bd2a64c8f2eab6295cc8ba0946c5c07c76f6d7200860fdd86dc9786d.
- Fixed required-header-poison.log SHA25601bef0449d47c639da40f4095570b2c2b2ebeab5dcee5bd1a26dbbd3fd1bddeb.
- Fixed compile_commands.json SHA2568554f63d6d424cc0303ec4c104b262864c975944c72f2a5fe98f3d901a78ee87.
- Actual staged audio_console.h SHA256cc872103790a9b2970cc3246f0b6b9ca9c22c4fb70b1f3c921c497acfde3537d.

Earlier no-compiler docs530 strict MkDocs, Python AST and git diff --check all exit0. Static literal-header audit assumptions and287/278 base counts are preserved in source-ready packet; only3 unresolved edges pointed to this one omitted header. No broader SDK omission or all-core usability result invented.

## Review and next action

Manager independently inspect exact descendant/source and raw old/fixed/poison/restored logs/provenance; integrate only after acceptance, rerun owning qindaqt.audio-installed-sdk-closure on integrated tree. Audio backend repair is Astra-owned separately. Signed Portage desktop package and actual laptop installed client/control checks follow accepted integration; never hand-copy installed headers. Immutable r16 and source2188 remain unchanged. Claude WIP preserved in prior worktree. No Windows/toolkit/native follow-on started.
