# Core SDK claim

- Exact base: a462c415a3678dc8dc96f15bd996abdceea20189
- Worktree/branch: everyday-core-sdk-20261008 / worker/everyday-core-sdk-20261008
- Outcome: usable public installed core clients, beginning with proven Audio header closure failure.
- Production ownership: src/services/audio_protocol/CMakeLists.txt only. Proposed focused own test directory plus additive owning tests/services/audio_protocol/CMakeLists.txt registration awaits manager confirmation. Owning audio-service wiki closure documentation.
- Finding: audio_types.h, console_model.h and vban_store.h include existing audio_console.h, omitted from FILE_SET. Literal static audit287 exported service headers/278 QindaQt include edges finds no other unresolved dependency.
- Preparation: build actual production AudioProtocol+AudioClient in isolated install fixture, use cmake --install FILE_SET, separately compile/link with stage-only includes/libraries, remove audio_console.h and require failure, restore and require pass. No source-header copy/fallback, no system include modification.
- No compiler/native lease; Astra owns current native Audio repair. Claude uncommitted WIP preserved in prior isolated tree. No install, host service or live bus action.
