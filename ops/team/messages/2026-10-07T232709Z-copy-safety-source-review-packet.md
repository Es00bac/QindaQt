# Initial copier safety source review packet — native gates pending

- Owner: ed-foreign-astra-20261007.
- Exact base: 1a205444cff9264ac0b21a3968506133132db5bc.
- Isolated branch: worker/everyday-copy-safety-20261007.
- Outcome: preserve existing/foreign/partial copy output after failure, with truthful typed failure presentation.
- Proposed ADR0357 supersedes only automatic failed-copy cleanup. No cross-device Move, recovery journal, source retirement, permanent-delete or Trash policy change.
- Resource state: source/static work only; root retains native/compiler/private-fixture lease.

## Actual prior reproduction

Immutable f5eac3f434c6ade4f8e4910a4524ab72c2e9b0cb test packet against actual production1a205 compiled strict with -j24 -l24. Four exact old-source sentinel cases failed (existing file/tree EEXIST, cancellation after replacement, vanished source after replacement): 2 setup/cleanup pass,4fail,0skip,exit4. Prior62f133 record and ignored copy-safety-old archive/logs preserve actual evidence. No old production file was modified for that run.

## Source and boundary

Both helper and Copy backend automatic deletion paths are removed. Actual written descriptors supply bounded output evidence; metadata readback distinguishes retained partial/completed traversal, changed destination and unconfirmed ancestry. Written identity and path observations confer no open/delete/restore authority. Directory mkdir/open is explicitly not atomic creation evidence; exclusiveCreation is false for directories. Source postcheck can fail even after complete copying and preserves output.

The GUI-thread controller value-copies request-ordered attempted success/failure and unattempted suffix. It clears stale output on the next admitted operation, preserves success-prefix facts, and refreshes observed partial effects through existing refresh-only consumers. No successful-copy framing or new recovery action is added to the failure card. Both banner Labels use plain text; full output paths remain accessible literal text.

New private safe_tree_copy.cpp/safe_tree_access_p.h split cohesive copy and descriptor ownership from unchanged explicit deletion traversal. New mutation_controller_results.cpp owns terminal receipt/presentation conversion. Root approved these paths and minimal CMake additions. Public/private interfaces document threading, lifetime, bounded evidence and compatibility. Source shape: copy332, descriptor126, deletion147, coordinator467/results89, backend448 nonblank (all below500).

## Verification actually run

- tools/validate-docs:525 Markdown documents/navigation,exit0.
- mkdocs build --strict --site-dir .cache/copy-safety-docs:exit0.
- git diff --check:exit0.
- cmake -DSOURCE_ROOT=<this tree> -P tests/apps/file_manager/check_mutation_boundary.cmake:exit0, including its planted forbidden-dependency matcher control.
- Exact old/new explicit deletion/Trash traversal text comparison: unchanged apart from an extraction blank line, then normalized without code changes.
- The new C++ and production-QML fixtures are authored but NOT compiled or run yet. No repair acceptance/execution claim.

## Requested next gates

Different-author exact static review by root, followed by a leased strict focused native build and tests: copy safety, local mutation, home Trash, existing controller, new mutation output, file-actions mutation where linked dependencies permit; production shared banner QML at normal/2x; boundary, affected application build and existing mutation UI checks. Tests retain all four original sentinel assertions, add source-postcheck retained output, destination-parent substitution and successful nested copy; typed controller cases cover partial/replaced/unconfirmed, batch prefix/suffix and cancellation; QML asserts hostile literal path and dismissal-only failure action.

No source-only stopping is intended: this first repair must be qualified and integrated before production work on accepted bounded ED05 recovery design021.
