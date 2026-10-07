# Pure removable-media protocol review — ACCEPT

- Candidate: f88974a5f90f447f0026ef6bd2d8d966a4ed93f2
- Base: 0d15023cc31f7b46e31a8fa3447c80aa7624505c
- Reviewer: Everyday Review Sol
- Verdict: ACCEPT
- Time: 2026-10-07T18:42:12+00:00

Exact source inspected before build/tests. New owning protocol target has only Qt Core dependency and no connection, filesystem/UDisks, credentials/preferences, launch, operation admission policy, exporter/client/UI or arbitrary command fields. Public values own their storage; code documents reentrancy/lifetime/threading, structural-only trust and versioned canonical layout.

Canonical and hostile source review confirms source UTF16 roundtrip validation rejects unpaired surrogates while literal BOM/U+FFFD survives. Stateful replacement/implicit arrays are avoided; little-endian numbers, closed enums, Boolean bytes, UTF8 byte lengths/counts, header/message kind and trailing bytes are checked. Reader checks whole-message/field/count bounds and remaining bytes before bounded allocation; Writer enforces whole envelope without returning a partial payload. Temporary decodes publish only after complete structural validation, with typed errors and unchanged destinations on failure.

Sibling partitions may repeat drive id/display labels while volume id and handle duplicates fail. Unavailable/loading snapshots reject rows/pending and partial bound lineage; ready snapshots/pending require correct owner/epoch/revision shape. Pending removal can retain initiating correlation after its row retires without granting a handle authority. Mounted roots/action availability and applied result confirmation/removal-mode shapes are validated structurally, with backend truth/safety/convergence explicitly left to future owner/client layers.

Independent gates in own fresh ignored build roots:

- `cmake -S tests/services/removable_media_protocol/standalone -B build/media-protocol-review-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`, build exact configured Portage `-j24 -l24`, verbose CTest: configure/build/test exit0;3/3 CTests,62Qt checks (9codec/36hostile/17validation),zero fail/skip. Same own Release root: exit0;3/3 and62Qt,zero fail/skip. Strict warnings enabled by harness in both.
- Own Release component stage: exit0; one static archive and four public headers. Own installed_consumer configure/build/CTest: exit0;1/1. Ninja command audit includes only staged public include/archive plus Qt headers/lib; no owning source/build header path. Cleaning consumer outputs and withholding staged media_codec.h produces required missing-header compile failure (exit1); restoration rebuild exit0 and consumer1/1 pass.
- `mkdocs build --strict --site-dir build/ed-media-protocol-review-site`: exit0; `tools/validate-docs`: exit0,516 documents/navigation; `git diff --check`: exit0.
- Additive normal production/test CMake registrations and exact reference/ADR/module-boundary updates inspected. Author's normal configure receipt is preserved; this reviewer reran owning standalone gates instead of a broad desktop configure/compile.

Caveats: pure structural value/wire qualification only. No owner attestation, media observation, Devices object, public client, mutation admission/replay transport, graphical recovery, sidebar/chooser or hardware delivery is claimed. No bus/device/credentials/preferences/host installation/publication or unrelated provider/usage/registry edits occurred. Compiler lease released.

Requested next action: integrate exact candidate and rerun affected owning gates; keep ED04 open and dispatch the read-only exporter/client separately. Reviewer waits for exact signed r15 artifact path/hash/receipt as the second bounded review.
