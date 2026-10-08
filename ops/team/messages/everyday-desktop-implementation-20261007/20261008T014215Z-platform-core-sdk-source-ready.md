# Core SDK source ready — native gate pending

- Exact base: a462c415a3678dc8dc96f15bd996abdceea20189; isolated everyday-core-sdk-20261008.
- Production diff: src/services/audio_protocol/CMakeLists.txt adds existing public audio_console.h to FILE_SET. No API, runtime service or graph change.
- Tests: owning tests/services/audio_protocol/CMakeLists.txt additive registration qindaqt.audio-installed-sdk-closure, run_installed_audio_sdk.py, installed_sdk_fixture/CMakeLists.txt, installed_sdk_consumer/{CMakeLists.txt,main.cpp}.
- Doc ownership: only appended Installed public client header closure paragraph/section in audio-service wiki; Astra retains graph/rejection/native prose.

Static inventory assumptions: literal include/qindaqt header paths in src CMakeLists containing FILE_SET HEADERS and install(. At original base287 service exported headers had278 QindaQt include edges; only3 unresolved edges all audio_console.h (audio_types, console_model, vban_store). Noninstalled keyring/portal internal header trees are not automatically declared public SDK omissions. This audit is not configured package-export qualification.

The new gate builds real production AudioProtocol+AudioClient and cmake installs actual FILE_SET plus exported targets. Separate consumer gets staged include/libs and a named all-QindaQt public-header poison fallback before host includes; compilecommands reject production source includes. A valid inactive public transport/client pair links and runs without start(), service activation or PipeWire. Missing audio_console.h must fail at poison, then restoration must build/run. Every child records exact argv/exit/log. Portable cmake builds have no hardcoded jobs; optional --native-makeopts forwards harness-selected native flags unchanged. Session/system bus addresses in fixture children are nonexistent private paths.

Direct no-compiler gates: Python AST exit0, git diff --check exit0, tools/validate-docs530 exit0, mkdocs build --strict --site-dir .cache/core-sdk-docs exit0. No native/compile gate yet; source is intermediate reviewable slice, not accepted complete milestone.

Requested next lease: run this gate with actual configured portageq MAKEOPTS unchanged. Preserve old exact-base Audio modules from git archive in ignored output and require old stage consumer failure before corrected positive/poison/restored gate. Parent actual installed laptop initial SDK failure is preserved by root; no own installed failure claim. No source-header fallback, installed include modifications, software installation or service mutations.
