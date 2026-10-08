# Exact Audio image/lifecycle source review — NEEDS_FIX

- Source: `99295b5df3c20f63086050b1332a9d6cd95badc4`; author receipt-only descendant `c6564ce716d9b850d5367bcbda9b06536915f858`.
- Reviewer: stable ed-foreign-astra employee, different from implementer; fresh isolated checkout after explicit qinda hub fetch.
- Verdict: **NEEDS_FIX**, source only. No compiler, Core, runtime, Portage, package, host graph or service action executed.

## Blocking finding

`tests/services/audio_service/tst_console_core_scope_runtime.cpp` contains a literal backslash-n between `missingImageAdmissionRefusesBeforeSlotInsertion();` and the next declaration. This is invalid C++ outside a string. It prevents the owning experiment from compiling; this is direct source evidence, not an executed compiler result. Media acknowledged the escaping mistake, preserved original992 and is preparing the exact minimal repair.

## Completed independent inspection

Read all new receipt/package/bootstrap/process scripts, target registrations, image-file/symbol/maps proof, native fixtures, production factory delta and owning architecture/ADR/README. No additional blocking source finding was found within this private diagnostic scope.

- The only prospective production delta requires a borrowed synchronous admission callback before temporary Core registration and again before the fixed public module factory. It does not wire the production worker or provide a production image authority implementation.
- The diagnostic receipt pins prepared upstream/patched module source, recipe/archive and actual Portage-image artifacts. Portage gpkg decompression verifies its manifest/payload and requires signatures for normal images before comparison/receipt. Direct installed Portage implementation was inspected; this was not a package execution.
- Pre-exec bootstrap selects the retained library/module/SPA image. Native checks compare exact regular files, current Core symbol and mapped inode, actual module filename/mapping and held private child mappings. Admission is checked before private Core/module activity; version strings alone do not grant it. Mismatched loaded modules abort the private diagnostic rather than entering unqualified teardown.
- The allocator negative is confined to the lifetime executable. Actual frozen upstream stream.c consumes properties on allocation failure, consistent with the interposer, and module-loopback registers hooks before setup_streams. The counter must witness the real failure path; no fake module implementation substitutes for upstream code.
- Module-first/Core-first, borrowed/owned producer, failed initialization, 64 churn and override fixtures were read with listener-storage lifetimes. The old control requires an actual ASan heap-use-after-free naming module-loopback.c; a timeout/assertion/compile failure does not qualify.
- The driver retains its fresh process-group leader unreaped with Linux waitid(WNOWAIT) through group signaling. It never signals a recycled bare PID after losing the waitable leader. This is owned group cleanup, not proof that arbitrary independently daemonized descendants are absent.
- The Portage recipe invokes explicit build/test/image/package phases, never merge/preinst/postinst/world/profile/service actions. Private bus/XDG/PipeWire environment and exact installed flag/USE baseline are explicit. Missing prerequisites or signature policy fail the gate.

## Limits and next gate

Images must stay immutable under manager ownership during qualification; hashing is not concurrent-writer exclusion. The payload comparator covers file type, mode, bytes, symlink target and complete entry set, not ownership/xattrs/hardlink topology. Full gpkg integrity/signature verification is distinct from claiming those additional filesystem metadata properties.

Five recipe files independently AST-parse without import/execution. Whitespace check passes. Documentation results follow in the repaired recheck receipt. No native test count, package result, ASan fault, installed dependency or gain realization is claimed. After the minimal exact repair, review its full diff before authorizing only the separately manager-routed package/private qualification; normal/ASan builds, seven positive rows, real old red, receipt poison/restoration, production integration/SDK and physical gates remain required.
