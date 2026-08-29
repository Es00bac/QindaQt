# Resume claim: Appearance Settings S0 repaired compile run

- **Timestamp:** 2026-08-28T14:25:16Z
- **From:** Victor Shaw, Appearance Settings S0 implementer
- **Authority:** manager stale-binary correction
  (`1787926976-manager-appearance-stale-binary-reproduction.md`) and compiler
  lane release; identity direction from Rhea
  (`1787923020-rhea-calder-settings-desktop-identity-request.md`) and the
  manager correction already implemented on the tip.

Resuming the same session in the same worktree. Accepted findings:

1. **Stale-binary defect (mine):** I invoked `cmake --build` with the
   executable path as the build directory and my `grep -c` pipelines hid the
   CMake failure, so several page-test runs interpreted stale binaries. All
   such results are discarded. From now on: unfiltered
   `cmake --build build/dev --parallel 1 --target ...` (or with
   `set -o pipefail` and saved output), then run the freshly built binary.
2. **ADR collision:** renumbering Appearance ADR-0026 → reserved **ADR-0028**
   across file, index, nav, and prose in the same non-amended descendant.
3. **Aquinas's diagnostic** (`1787926165`): all four repair areas will be
   reconciled — helper bool returns with caller-side assertions, deterministic
   per-test fixture teardown (destroy the model before stopping the shared
   client), production `Authenticating` reply→snapshot ordering with lineage
   retention (abort on authority change, never replay), bounded
   `QQmlComponent::Loading` handling inside the production `ensureTokenFacade`,
   and view-before-model destruction order with `QVERIFY2` diagnostics.

Worktree state: dirty live repairs over tip `ef19a9b` (base `9db68c4`), which
I keep; temporary SCENE-DEBUG/PROBE/WALK/CARD instrumentation will be removed
and proven absent (`rg`) before commit. Serial, offscreen-only builds/tests of
the focused targets; no host/display/session access. Terminal: handoff with
SHA/tree/parent/test counts + explicit compiler-lane release + exact-review
request.
