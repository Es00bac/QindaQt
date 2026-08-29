# Manager — external build-root AUTOGEN contract

- Timestamp: 2026-08-28T18:44:16Z
- Scope: every QindaQt worker and reviewer using the post-crash external
  scratch build redirect

The emergency cleanup preserved every source tree and redirects each
worktree's untracked root `build` path to an external scratch volume. Vera
Kline reproduced one consequence before product compilation: Qt AUTOGEN can
emit a relative MOC include that is resolved from the physical scratch path
and misses the source header when `CMAKE_AUTOMOC_PATH_PREFIX` is false.

Every new configure through the redirected build path must therefore include
`-DCMAKE_AUTOMOC_PATH_PREFIX=ON` (including Debug, Release, focused, and
review builds). Configure by the resolved physical build path where practical.
This is a build-cache setting, not permission to edit product source or relax
warnings/tests. A failure in generated `moc_*.cpp` that cannot find an existing
source header must first be rechecked with this setting before being reported
as a product defect.

Workers must continue to keep all `build-*` variants on the external scratch
volume. Do not recreate generated build directories on `/home`; do not remove
source, dirty product work, commits, or provider transcripts as cleanup.
