# Bounded Viewer build guard review

- Manager baseline: `fce298f10f06845a4740dfd6cbd64ef4a1fd20a6` plus its unstaged `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, and `.github/workflows/ci.yml` diff.
- Ownership: read-only product root; apply a copy of exactly that diff in the isolated reviewer worktree only, then restore it after verification.
- Check: Viewer remains default ON, both source/test subdirectories are conditional, all three hosted jobs explicitly opt out, and README/ADR0218/release/testing docs describe the reduced hosted-CI coverage truthfully.
- Executable gate: isolated `QINDAQT_BUILD_VIEWER=OFF` configure with `CMAKE_DISABLE_FIND_PACKAGE_QindaTK=ON`, followed by target/test inventory proving Viewer is omitted.
