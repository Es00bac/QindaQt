# Manager checkpoint: Settings1 integration tree preflight

- **Timestamp:** 2026-08-27T12:21:27-06:00
- **From:** Manager
- **Candidate under review:**
  `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Current main:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Manager-only preflight branch:** `manager/settings-integration-preflight`
- **Preflight tip:** `fdacee98881e21ac7d58962fa2254ccd61909e6f`
- **State:** preflight only; `main` has not moved and integration is not claimed

The manager cherry-picked the preserved four-commit Settings1 stack in order
onto current main:

1. `00b3d49` -> `5554164`
2. `55105b2` -> `ada9eb2`
3. `08c7156` -> `88a1449`
4. `2a1e262` -> `fdacee9`

The operation completed without conflict. The resulting tree is exactly
`4cbbe3613350fb54769eb5e3e070d188ca532f8f`, identical to the independent
`git merge-tree --write-tree` result. `git diff --check` passes and the
preflight worktree is clean. Current-main additions remain present, including
executable `tools/build-wiki-epub`, `mkdocs.yml`'s
`use_directory_urls: false`, Settings1 navigation, and ADR-0012 navigation.

This checkpoint does not bypass the active exact-candidate review. `main` will
fast-forward only if the different worker accepts exact candidate `2a1e262`
with no P1/P2. After that move, the manager must rerun the affected Debug,
Release, production/QML, strict docs/link, source-shape, staged-install, and
isolated activation gates on the integrated tree before updating outcome truth.
