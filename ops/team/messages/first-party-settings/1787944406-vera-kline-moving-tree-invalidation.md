# Vera Kline invalidated exact attribution after manager-tree movement

- Timestamp: 2026-08-28T19:13:26Z
- Original QA HEAD: `631fa4404fdee1d22a3bfe7ed12b436ea9b6b2b1`
- Newly observed HEAD: `c477627b7a3d0a01d4e3f83106a5b806207a4564`
- State: working; latest-tree rebuild and retest underway

The manager worktree advanced by seven Audio/registry commits while the strict
build and CTest were running. The original five dirty paths were also replaced
by current edits to `ops/team/providers.json`, `src/CMakeLists.txt`,
`src/shell/audio_applet/CMakeLists.txt`, `tests/CMakeLists.txt`, and
`tests/shell/audio_applet/CMakeLists.txt`.

The completed evidence is green as a moving-tree checkpoint: 1,391/1,391
strict build actions; full serial CTest 223/223 in 84.41 seconds; source shape
1,250; docs/navigation 84; strict isolated MkDocs; JSON/YAML, added-marker,
committed/worktree diff and whitespace checks. It is not exact-current-HEAD
evidence because source state changed underneath the run.

Current `c477627b` remains an additive public descendant with zero unmerged
entries, zero untracked source, and clean committed/worktree diff checks. I am
reconfiguring and rebuilding the current dirty tree, inventorying/running all
new registered tests, repeating strict static gates, and checking exact
HEAD/status stability before issuing any commit-safety verdict. The manager was
asked to freeze this worktree if an exact-current-tree verdict is required.
