# Dorian Vale exact review: production-surface candidate PASS

- **Timestamp:** 2026-08-28T04:01:58Z
- **Reviewer:** Dorian Vale — OpenAI Codex `gpt-5.6-sol`, reasoning high;
  QindaQt KWin API and nested-session evidence auditor
- **Candidate:** `6b57ef3c34d12967df837333a6cfb0ab1a7f5acd`
- **Tree:** `c576b53ec935ba112a02db410bed69dac331a08d`
- **Parent/base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Verdict:** **PASS — zero P0, P1, or P2 findings**

I independently reviewed the immutable six-path diff and reran its relevant
compiler, pure, and private nested evidence. Mira Quill's handoff was treated
as input, not approval. The detached review worktree remains tracked-clean at
the exact commit/tree/base.

## Contract audit

- The dedicated fixture at
  `tests/session/fixtures/shell_surface_profiles/qindaqt-surface-proof.json:1-37`
  is schema v1, contains exactly the qualified `command-bar` and `smart-shelf`,
  and is byte-for-field equivalent to `data/profiles/qindaqt.json` after
  excluding identity/prose and `hideMode`. The production profile remains
  unchanged: its shelf is still `intelligent`; only the test fixture makes both
  panels `never`.
- `tests/session/CMakeLists.txt:165-195` binds all three rows to the exact
  fixture directory and ID `qindaqt-surface-proof`. The Python preflight at
  `test_shell_surface_nested.py:68-109,151,184-203` resolves
  `<profile-dir>/<profile-id>.json`, requires declared ID equality, exactly two
  expected panel IDs, and `never` for each before starting KWin. The probe at
  `shellsurfaceprobe.cpp:235-256` requires the environment value and passes it
  unchanged as the production shell's `--profile` argument.
- The preflight intentionally owns only proof identity/intent. Full schema
  truth is checked by the unchanged production `ProfileCatalog`/`ProfileLoader`
  before surface initialization; an offscreen `qindaqt-shell --list` invocation
  loaded exactly `qindaqt-surface-proof`, exit 0. Adversarial direct checks
  rejected declared-ID mismatch, one-panel input, `intelligent` hiding, and a
  wrong panel set.
- There is no packaging leak. The fixture exists only below `tests/session`;
  `src/profiles/CMakeLists.txt:38-41` still installs only `data/profiles/`.
  Generated install scripts contain no fixture reference; generated CTest files
  contain the expected three test-only references. The production binary has no
  unresolved `ldd` dependency.
- The runner clears inherited host D-Bus/display and development-mutation
  markers, creates disposable XDG roots plus a mode-0700 runtime directory, and
  supplies no `--test-scenario`. An adversarial environment check confirmed
  those removals and mode. The shell subprocess receives explicit profile and
  theme arguments; the production surface, layout, visibility, and applet paths
  are otherwise unchanged.
- Existing fail-closed protocol evidence remains causal, not screenshot or
  timing inference. The active snapshot requires two unique live roles on one
  explicit `wl_output`, committed layer/anchor/edge/zone/size state, and exact
  `configure < acknowledge < non-null attach < commit` ordering. It is frozen
  while the reduced maximize area is observable; the final trace separately
  fences teardown identity.
- The changed documentation is truthful. `testing-harness.md:205-248` and
  `panel-surfaces.md:127-144` call this a deterministic initial-publication and
  work-area proof and explicitly do **not** claim intelligent-hide transitions,
  animation, partial panels, or heterogeneous outputs. Thus the fixture removes
  the historical race without masking or weakening the product policy claim.
- Modularity is preserved: no production module changed, the fixture validator
  remains a bounded runner preflight, and source-shape reported 831 files with
  zero allowlisted skips.

## Independent evidence

- Fresh `cmake --preset dev`: exit 0.
- Serial build with worktree-local `TMPDIR/TMP/TEMP`:
  `qindaqt-shell-surface-session-probe`, production shell/launcher dependency
  graph, protocol trace, runtime-options, and profile targets: **242/242** plus
  the two subsequently selected profile validation targets **8/8**, exit 0.
- Focused selector for three profile tests, runtime options, protocol trace,
  nested-scenario parser, and Python syntax: **7/7**, exit 0.
- Production-shell catalog load of the exact fixture via `--list`: exit 0.
- Private nested selector, fresh short
  `/home/cabewse/.cache/qst-review.34543I`, mode 0700,
  `--parallel 1 --stop-on-failure`: **3/3**, exit 0, 6.18 s:
  - 1920x1080 -> 1920x996 -> 1920x1080;
  - 1920x1200 -> 1920x1116 -> 1920x1200;
  - 2560x1440 -> 2560x1356 -> 2560x1440.
- Each retained marker reports `passed=true`, Wayland, one common output,
  `identityAmbiguous=false`, `protocolAmbiguous=false`,
  `inputTruncated=false`, two mapped layer-2 dock roles, exact 30/54 zones,
  causal orders, bounded shell start/stop, and restored work area. Shelf widths
  are the exact rounded 52% values: 998 at width 1920 and 1331 at width 2560.
- `python3 tools/docs_validation.py`: 47 documents/navigation valid, exit 0.
  `tools/check-source-shape`: 831 files, zero skips, exit 0.
  `git diff --check` and `git show --check`: exit 0.
- `mkdocs build --strict` remains unavailable because no `mkdocs` executable is
  installed; the repository documentation validator passed.

Two reviewer setup stops preceded the green reruns and are not product
failures: the first focused CTest invocation selected two profile executables I
had not yet built (one test passed, the next was Not Run), and my first
adversarial helper passed a nonexistent parent to an API whose runner contract
uses an existing `TemporaryDirectory`. I built the missing targets and reran
7/7; I created the expected parent and reran the adversarial matrix successfully.

After the nested selector, the exact short root was empty, removed with `rmdir`,
and verified absent. No review-owned KWin, Xwayland, qindaqt-wm, shell, probe,
CTest, compiler, or build process survives. The host compositor/session/input,
active bus, display configuration, unrelated `/tmp`, and physical outputs were
not touched.

## Next action

The compiler/private-runtime lane is released. The manager may integrate the
exact non-amended candidate, subject to the manager-owned integrated-tree gates.

