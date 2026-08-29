# File Manager S0 exact integration-preflight handoff

- Time: 2026-08-28T14:11:41Z
- Reviewer: Curie the 2nd, read-only integration-preflight reviewer
- Candidate: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
  (tree `5e60d151d53cf6ed391e0a765e3a27da14e4a5c9`, parent
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`)
- Current public: `cbec6fb42216e5bcc3283004473be7f5f6ccda66`
  (tree `15d4e9a9457b90e32c4dffa9b3720d8012fbb7de`)
- Scope: integration preflight only. This is not Juno Park's correctness
  review and does not approve or reject the candidate.

## Exact conflict and drift result

The merge base is exactly candidate parent `9db68c4`. Immutable
`git merge-tree --write-tree --messages cbec6fb 9ca240c` returned exit 1,
synthetic tree `f6c16f066c2d261426f866d6d01c88b8e9ae52ef`, and exactly three
content conflicts:

1. `docs/wiki/adr/index.md`
2. `docs/wiki/index.md`
3. `mkdocs.yml`

Deterministic resolutions:

- ADR index: retain ADR-0027 then ADR-0028 in numeric order. Remove the
  candidate-base sentence saying ADR-0027 is an in-flight gap; ADR-0027 is
  integrated public truth.
- Wiki index: retain the complete AppShell entry followed by the complete File
  Manager entry.
- `mkdocs.yml`: retain both Application entries and both ADR-0027/ADR-0028
  decision entries in numeric order.
- Preserve the automatic additive unions in `src/CMakeLists.txt`,
  `tests/CMakeLists.txt`, and
  `docs/wiki/architecture/module-boundaries.md`. They retain current AppShell,
  Power/Brightness, and the new File Manager registrations without conflict.

The exact path sets are 29 candidate-only paths, 59 public-only paths, and six
shared paths. Candidate-only product work is confined to
`src/apps/file_manager/**`, `tests/apps/file_manager/**`, its page, and
ADR-0028. Public-only product work is AppShell and Power/Brightness plus their
tests/docs/task evidence. There is no source file or target-name collision.

## Blocking findings before integration

### P1 — Installed executable is not hermetically loadable/proven

`src/apps/file_manager/CMakeLists.txt:43-60` compiles the manager build root's
absolute `${CMAKE_BINARY_DIR}/qml` into the production executable and directly
links shared `QindaQt::TokensQml`. `main.cpp:153-159` unconditionally adds that
build path. The Tokens backing library installs below
`${QT6_INSTALL_QML}/QindaQt/Tokens`, while the executable installed in
`${CMAKE_INSTALL_BINDIR}` has no `INSTALL_RPATH`. The FileManager component
also installs only the executable, desktop file, and themes; it does not carry
Tokens/Controls artifacts. The candidate has no staged-installed executable
test.

This creates two unacceptable pre-integration uncertainties: the dynamic
loader may fail before `main()`, and a staged run on the build host may pass by
silently importing build-tree QML instead of installed payload. Engine import
search comments do not solve ELF loading or hermeticity.

Minimal repair requirements for Ada's non-amended descendant:

1. Do not embed or unconditionally add a machine-specific build-tree QML path
   in the installed production binary. Put build-only import discovery in the
   development/test launch environment or another explicit build-only seam.
2. Give the executable a relocatable installed loader path to every directly
   linked QML backing library, or refactor the direct shared-library dependency
   behind an equally tested relocatable boundary. Do not rely on `LD_LIBRARY_PATH`.
3. Add a disposable staged-install test that clears ambient/build QML and
   library paths, requires the exact executable/desktop/theme/dependent QML
   payload, runs `--check-theme`, and constructs the File Manager QML root
   offscreen with a deterministic exit. It must fail if it can see the build
   tree. State the FileManager package/component dependency on Tokens/Controls
   explicitly if they remain separate install components.

### P2 — New normative documentation is false on current public

`docs/wiki/apps/file-manager.md:15-18,166-168` and
`docs/wiki/adr/0028-file-manager-bounded-local-launch.md:39-41,61-73` call
AppShell in flight, unreviewed, or not integrated and set AppShell integration
as a future revisit trigger. Public `cbec6fb` already contains accepted,
executable AppShell S0. In the resolution/repair descendant, preserve the
implemented fact that File Manager S0 does not consume AppShell, but state that
AppShell is integrated and the migration is an explicit later File Manager
slice. Update the obsolete revisit trigger rather than leaving a condition
that is already true.

No P0 was found. Configure/QML-tooling and runtime correctness remain unproved,
not passed: the candidate's `add_dependencies` supplies order but must be
validated to ensure the foreign Controls import resolves during the actual Qt
QML build.

## Serialized integration gate order

After Juno's exact correctness review and Ada's repaired exact descendant:

1. Resolve the three documentation conflicts exactly as above on current
   public; confirm the auto-unioned root registries still contain AppShell,
   Power/Brightness, and File Manager.
2. Configure one clean dependency-light build with testing on, KWin plugin off,
   production shell off, and host-uinput tests off. A successful existing build
   cannot substitute for this combined-tree configure.
3. Build serially (`--parallel 1`) in this order:
   `qindaqt-file-manager`, `qindaqt_file_manager_history_tests`,
   `qindaqt_file_manager_local_lister_tests`,
   `qindaqt_file_manager_launch_intent_tests`, then
   `qindaqt_file_manager_controller_tests`. The app target must pull Tokens and
   Controls QML plugin prerequisites; missing QML imports or generated targets
   are failures, not warnings to suppress.
4. Run `ctest --test-dir <build> -L file-manager --output-on-failure
   --no-tests=error`; require all four models plus desktop metadata, both CLI
   rejection rows, and the new staged-installed row.
5. In a private disposable staged prefix, run the installed checks described
   in P1 with sanitized XDG/QML/library environment and
   `QT_QPA_PLATFORM=offscreen`. Never use the host display/session.
6. Rerun current-public dependency/regression selectors:
   `^qindaqt\.(design-tokens-|controls-|app-shell-)` and
   `^qindaqt\.(power-protocol-|power-aggregation-|brightness-model-)`.
   Then run the broad dependency-light CTest registry with
   `--no-tests=error`; the shared registries make a passing File Manager label
   alone insufficient.
7. Run `tools/check-source-shape`, `tools/validate-docs`, strict MkDocs, and
   `git diff --check` on the final combined exact tree. Confirm both ADR pages
   and both application pages are navigable.

## Commands and preservation evidence

Read-only commands used: `git merge-base`, `git merge-tree`, `git diff
--name-only/--check`, `git show <commit>:<path>`, `git grep`, `git ls-tree`,
`git cat-file`, `git rev-parse`, and `git status`. I also read Ada's exact
handoff and the applicable wiki/operating documents. No compiler, configure,
test executable, GUI, session, input, display, or host-user-filesystem action
was invoked.

Final status evidence:

- Preflight worktree: clean detached `cbec6fb`, tree `15d4e9a`.
- Candidate worktree: clean `worker/file-manager-s0` at `9ca240c`, tree
  `5e60d15`.
- No product path, branch, ref, candidate commit, or user work was altered or
  discarded.

Requested next action: route P1/P2 plus any independent correctness findings
to Ada in the preserved File Manager worktree, create a non-amended repair
descendant, then exact rereview and execute the serialized gates above.
