# Mina Shah claim: public API/docs/acceptance-trace audit (pod item 3)

- **Timestamp:** 2026-08-27T17:53:00-06:00
- **From:** Mina Shah, Claude Sonnet 5, read-only Display D1 public-API/docs/
  acceptance-trace reviewer
- **To:** Display D1 lead/keeper, manager/router, Iris Hale, Claude Fable
- **Authority:** `1787873857-display-d1-readonly-pod-assignments.md` item 3,
  amended by `1787865730-manager-next-d1-pure-display-outcome.md` and
  `1787859005-manager-fable-display-decision.md`
- **Exact worktree/base:**
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1` at
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, read-only to me

I claim item 3: once the Display architecture/reference pages and ADR-0015/
0016 land, I will independently map all seven required contracts and every
acceptance row to a public API, focused test, and authoritative doc statement,
covering ownership/lifetime/thread/error/compatibility, injected-port pre/
postconditions, pure dependency direction, forbidden-artifact absence, source
shape, and selector/navigation truth.

## Current state

`docs/wiki/architecture/display-service.md`, `docs/wiki/reference/display1-v1.md`,
and ADR-0015/0016 do not exist in the worktree yet; `docs/wiki/adr/index.md`
still ends at ADR-0014, and `mkdocs.yml` has no display entries. This matches
the lead's own triage note that "docs/ADRs and minimal registries remain
before the source checkpoint," so the full mapping audit is genuinely blocked,
not omitted. I will resume it the moment those land.

## Preliminary header/CMake-only pass (ahead of checkpoint)

To avoid idling, I read every public header and each module's own
`CMakeLists.txt` under the four owned modules. Findings so far:

1. **Dependency direction is pure and matches contract.** `display_protocol`
   links only `Qt6::Core`/`Qt6::DBus`; `display_identity` and
   `display_topology` link only `Qt6::Core` (+ `DisplayProtocol` for
   topology); `display_transaction` links `DisplayProtocol`,
   `DisplayTopology`, `Qt6::Core`. No KWin/Wayland/QML/filesystem/logind/
   libkscreen/provider-QObject import anywhere, and no service/name/XML/
   client/UI/journal-file artifact exists under the repo
   (`grep -rl "org.qindaqt.Display1"` outside the four owned trees: none).
2. **Source shape is well within bounds.** Largest file is
   `display_topology/src/topology_validation.cpp` at 383 non-blank lines;
   every other file is smaller. No 500-line decomposition review is triggered.
3. **Ownership/lifetime/thread documentation is inconsistent across sibling
   pure-value modules** (AGENTS.md requires this when not obvious from the
   type system, and most modules already set the precedent):
   `display_identity/identity_resolver.h:18-20`,
   `display_identity/identity_registry.h:58-62`, and
   `display_topology/topology.h:16-18` each carry an explicit
   borrowed-arguments/reentrant/thread-safe/no-side-effects statement.
   `display_protocol/display_validation.h` (no header-level contract comment
   at all) and `display_protocol/display_codec.h`/
   `display_transaction/transaction_journal.h` (fail-closed/no-partial-replace
   stated, but no explicit thread/reentrancy statement) do not carry the same
   statement despite being the same shape of pure function. Smallest repair:
   add one line mirroring `topology.h:16-18` to those three headers.
4. **Question, not yet a finding:** `display_validation.cpp:298-322`
   (`confirmationRequirement`) maps only `ChangeClass::Topology` to
   `ConfirmationRequirement::Required`; all twelve other classes —
   Brightness, Dimming, SdrBrightness, IccProfile, VrrPolicy, RgbRange,
   Overscan, DdcCiPermission, MaximumBitsPerColor, ExtendedDynamicRange,
   Sharpness, AutoRotatePolicy, CustomModeDefinition — are
   `BypassedForClosedPolicy`. This is exhaustively tested
   (`tst_display_protocol_values.cpp:188-196`, including an unknown-value
   fail-safe row) and is mechanically a closed switch, so it satisfies
   contract 7's "explicit closed policy value with tests" literally. But the
   accepted Fable decision only names "Class-B brightness/color fields" as
   provisional; it does not name DdcCiPermission, Overscan,
   MaximumBitsPerColor, ExtendedDynamicRange, AutoRotatePolicy, or
   CustomModeDefinition as Class-B. Please confirm at the checkpoint whether
   the reference page or ADR-0015 will name and justify each bypassed class
   individually (ownership per contract 7), or whether some of these twelve
   should move to `Required` before this is treated as accepted policy rather
   than a placeholder enumeration.

No compiler, configure, build, runtime, or host-state action was taken. I will
post the full seven-contract acceptance-row mapping as soon as the two pages
and ADRs appear in the worktree.
