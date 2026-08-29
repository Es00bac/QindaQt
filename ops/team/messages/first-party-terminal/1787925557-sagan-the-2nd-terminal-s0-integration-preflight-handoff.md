# Sagan the 2nd — Terminal S0 exact integration-preflight handoff

- Time: 2026-08-28T13:59:17Z
- Owner: Sagan the 2nd
- State: waiting; preflight complete, Juno verdict and manager action pending
- Candidate: `a15a5f24c6075fe855ac263739fde59dc008e122`
- Candidate tree: `20c720ab5c17e3e64395627406c3f37f4a311c29`
- Candidate parent: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Current public: `cbec6fb42216e5bcc3283004473be7f5f6ccda66`
- Public tree: `15d4e9a9457b90e32c4dffa9b3720d8012fbb7de`
- Exact merge base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`

This is an integration-mechanics/dependency preflight, not a duplicate source
correctness review. It does not approve Juno Park's immutable review subject.
No product/ref edit, commit switch, merge, configure, compile, CTest, package
install, PTY/UI/session/input, or compiler/private-runtime-lane use occurred.

## Exact ancestry and drift

- Candidate is one commit directly above the exact merge base. Public is a
  divergent descendant containing AppShell and PB-0. Neither tip contains the
  other.
- Public changes 65 paths from base; candidate changes 35. Six are shared:
  ADR index, module boundaries, wiki index, MkDocs navigation, and the source
  and test CMake registries.
- Candidate-only count is 29: 28 paths are absent from base/public and CI is
  byte-identical base/public. Public-only count is 59: 53 paths are absent from
  base/candidate and six are byte-identical base/candidate. Cross-side drift is
  zero on all exclusive paths.
- Candidate manifest SHA-256 reproduces exactly as
  `ce125927a2cba411ff0aef11dde61a97a9f6a15b44fa7aff73e3bac43e837040`.
- Read-only legacy `git merge-tree` reports six changed-on-both paths, nine
  marker lines, and therefore exactly three conflict hunks.

## Exact conflict resolutions

Only these three paths conflict textually. Any additional conflict is a stop:

1. `docs/wiki/adr/index.md`: retain ADR-0027 AppShell Accepted followed by
   ADR-0028 Terminal. ADR-0028's final status must match the accepted decision
   resolution below; do not drop or renumber either row.
2. `docs/wiki/index.md`: retain Text Editor, QindaQt.AppShell 1.0, and Terminal
   bullets in that order, preserving each tip's complete description.
3. `mkdocs.yml`: under Applications retain AppShell, Text Editor, Terminal; in
   ADR navigation retain both ADR-0027 then ADR-0028 in numeric order.

Three other shared paths auto-merge, but the manager must inspect the semantic
union before committing:

- `docs/wiki/architecture/module-boundaries.md`: preserve public AppShell,
  Power protocol, and brightness-model rows plus the candidate Terminal row
  immediately after Text Editor. Preserve both AppShell and Terminal
  dependency/authority paragraphs.
- `src/CMakeLists.txt`: preserve public `app_shell`, Power protocol, and
  brightness-model registrations and add candidate `apps/terminal` immediately
  after `apps/text_editor`.
- `tests/CMakeLists.txt`: preserve corresponding public test registrations and
  add `apps/terminal` immediately after `apps/text_editor`.

Before any manager truth update, all 29 candidate-exclusive blobs must equal
candidate and all 59 public-exclusive blobs must equal public. The only
intentional candidate-exclusive difference allowed afterward is ADR-0028's
status if the manager accepts it in the integration commit.

## Dependency preflight and P0-P2 findings

Counts: P0 `0`, P1 `0`, P2 `4`.

1. **P2 environmental gate blocker — dependency absent locally.** Local
   `pacman -Si qtermwidget` and `pacman -Fl qtermwidget` exit 0 and prove Arch
   `extra/qtermwidget 2.4.0-1` publishes `qtermwidget6-config*.cmake`, target
   metadata files, `qtermwidget6.pc`, headers, and `libqtermwidget6.so`.
   `pacman -Q qtermwidget` and both pkg-config names exit 1; the four expected
   `/usr` roots and package cache are absent. Because Terminal registration and
   `find_package(qtermwidget6 REQUIRED)` are unconditional, this host cannot
   configure the merged tree until the manager deliberately provisions that
   package or uses a matching clean Arch container. Metadata alone does not
   prove imported target `qtermwidget6`; configure/link must.
2. **P2 contract/configuration risk — 2.4.x is not enforced.** ADR-0028 lines
   41 and 65 say the audited integration contract is 2.4.x, but
   `src/apps/terminal/CMakeLists.txt:6` requests no version. Today's sync
   package is compatible 2.4.0-1; rolling CI can later admit an unaudited
   series. Micah/Juno should repair the constraint or record a deliberate
   fail-closed version policy before integration.
3. **P2 evidence mismatch — seven rows, not eight.** Exact Terminal test CMake
   declares four helper-created QtTest rows and three direct CMake rows, seven
   unique `qindaqt.terminal-*` names. The handoff claims eight. Acceptance must
   identify/add the intended eighth row or correct every expected-count claim;
   the manager must not report 8/8 from this registry.
4. **P2 decision-state blocker — mandatory dependency ADR is Proposed.** The
   documentation policy reserves Proposed for unresolved discussion/feasibility
   and Accepted for project commitment. If review, dependency provisioning,
   and executable gates pass and the project chooses qtermwidget, the manager
   integration change must set ADR-0028 and its index row to Accepted. If the
   project is not ready to accept it, do not integrate the mandatory target.

## Serialized manager build and test order

After Juno PASS/owner repairs, unchanged public-tip assertion, exact merge
resolution, and qtermwidget 2.4.x provisioning:

1. Start a fresh manager-owned dependency-light Debug tree using the public CI
   switches: testing ON, KWin plugin OFF, production shell OFF, host uinput OFF,
   and strict warnings ON. Record the exact installed qtermwidget version.
2. Build serially in fault-localizing order:
   `qindaqt_terminal_support`, `qindaqt_terminal_adapter`, `qindaqt-terminal`,
   `qindaqt_terminal_launch_policy_tests`, `qindaqt_terminal_session_tests`,
   `qindaqt_terminal_appearance_tests`, then
   `qindaqt_terminal_window_tests`. The dependency graph should link
   qtermwidget only through the adapter/executable; the four tests link support
   and Qt, never qtermwidget.
3. Run exact Terminal rows in this order: launch-policy, session, appearance,
   window-offscreen, desktop-metadata, CLI positional rejection, installed
   metadata. Require 7/7 unless a reviewed descendant deliberately adds the
   missing eighth row. The CLI and installed-theme probes are documented to
   exit before opening a window or PTY.
4. Inspect the staged Terminal component: executable, desktop file, and built-in
   theme data; require its isolated `--check-theme` probe. Inspect dynamic link
   truth so the installed executable resolves qtermwidget6 and no pure test
   binary unexpectedly gains it.
5. Run current-public focused regressions on the combined tree:
   `^qindaqt\.app-shell-` requires 5/5 and
   `^qindaqt\.(power-protocol-|power-aggregation-|brightness-model-)` requires
   6/6. Then run the broad dependency-light CTest suite because Terminal is an
   unconditional top-level addition.
6. Run whitespace, source-shape, documentation/navigation, strict MkDocs, and
   link gates. Validate the edited workflow syntax and both Arch dependency
   lists/version-record commands. The candidate changes CI only where public
   retains the exact base blob, so no CI content union is needed.
7. Reconfigure/build the Release production-shell job with its existing public
   flags to prove the new mandatory package does not break that global build.
   The Terminal change creates no proportional need for a new private KWin
   session by itself; the existing nested production rows remain CI-owned and
   may run only in their separately authorized serialized private lane.
8. A real qtermwidget adapter/PTY interaction is not among the seven registered
   rows. Do not infer UTF-8 rendering, keyboard byte flow, resize propagation,
   signal-exit truth, live teardown, first frame, or PSS from compile/offscreen
   proof. Run those only as a separate authorized Terminal live gate, or
   integrate this stopping point with product evidence no higher than WIRED.

## Deterministic manager sequence

1. Wait for Juno's exact verdict. Route any blocking candidate defect to Micah
   and preflight the immutable descendant again.
2. Assert public remains exactly `cbec6fb`; if moved, recompute every overlap
   and drift result.
3. Provision/record qtermwidget 2.4.x and settle all four P2 items.
4. In a clean manager worktree, create a two-parent non-fast-forward merge with
   public `cbec6fb` first and accepted Terminal candidate second. Resolve only
   the three declared conflicts and inspect all three auto-unions.
5. Prove exclusive-blob retention, run the serialized and static gates above,
   then set ADR status consistently with the actual project decision.
6. Commit only after passing evidence. Record exact merge SHA/tree/parents,
   dependency version, real test counts, installed manifest, and live-runtime
   caveat. Update manager-owned TASK/HANDOFF/QQ evidence from the integrated
   stopping point only; activity and this preflight add zero progress.

Sagan the 2nd is now waiting and available for a new exact public/candidate
preflight. No statement here is Juno's review verdict or candidate approval.
