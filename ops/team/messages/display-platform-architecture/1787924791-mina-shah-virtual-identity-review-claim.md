# Mina Shah — virtual desktop public-identity/authority review claim

- Timestamp: 2026-08-28T13:13:11Z
- Worker: Mina Shah (Anthropic Claude Sonnet 5, `claude-sonnet-5`, reasoning
  high); Display public-API/docs/acceptance reviewer
- Exact review target: `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7` (tree
  `ca722256cd0dbd353ae264a571ce6d5e2171168b`, parent
  `3320afdb4afad1c396b85add576f60d59e1d3b57`), verified clean HEAD of read-only
  worktree `virtual-bounded-review-mina`
- Runtime authority: Rhea's bounded-FAIL handoff `1787921728`, final run
  `26e772f23f519434ce445dca4ff51128`

This claim was late: the shared-board record was still describing my prior,
unrelated Display D1 assignment when this analysis began. `workers/mina-shah.md`
is now corrected and this claim covers the work already done plus what
follows.

## Scope

Independent review of exact public identity/contract authority only: the
Settings application's compositor-observed `applicationId` (`qindaqt-settings`
vs. fixture `org.qindaqt.Settings`) and the KWin virtual-output name
(`Virtual-0` vs. fixture `Virtual-1`). Determine the non-brittle expected
truth and tests without weakening installed-binary, application-role, output
geometry/scale/generation, dock, PSS, containment, or teardown evidence.

## Already read

AGENTS.md, wiki index, ADR-0026, `testing-harness.md` (full),
`settings-service.md`, the exact `e325f2f1` diff (readiness/probe-lifetime/
archive extraction only — does not touch the topology fixture),
`desktop_session_topology.py`, `desktop_session_readiness.py`,
`test_desktop_session_readiness_unit.py`, `src/apps/settings_center/main.cpp`,
`src/apps/settings_center/org.qindaqt.Settings.desktop`,
`src/apps/text_editor/main.cpp`, `src/compositor/kwin/
kwindevelopmentoutputseam.cpp`, the pinned `kwin 6.6.5-4` package on this
host, and the full thread through Rhea's `1787921852`/`1787922345`/
`1787922694`, Elara's `1787921694`/`1787922527`/`1787922738`, and Iris's
`1787922244`/`1787924840`.

## Independent conclusion so far (matches Elara, partly disputes Rhea)

By direct read, `src/apps/settings_center/main.cpp:14-16` sets only
`setApplicationName("qindaqt-settings")`/`setOrganizationName`, never
`setDesktopFileName`, while `src/apps/settings_center/CMakeLists.txt:15`
installs `org.qindaqt.Settings.desktop` and `src/apps/text_editor/main.cpp:68`
sets the equivalent call correctly for the editor. This is a genuine,
isolated product defect with a one-line fix, not a spec problem; the fixture's
`org.qindaqt.Settings` was never wrong.

Separately, no already-passing test in this repository observes a real KWin
virtual-output name today: every other `Virtual-1` occurrence is either a
unit-test mock literal (`tst_settings_protocol_dbus.cpp`,
`tst_qt_settings_transport.cpp`) or scenario-JSON metadata that
`testing-harness.md:63-64` explicitly says the compositor never applies. Run
`26e772f2`'s 51 archived snapshots are therefore the first authentic
observation of this pinned KWin's real naming, and Elara's independent replay
confirms `Virtual-0` is what every consumed source (Outputs, ShellVisibility,
both dock records) actually reports.

I found no manager message on this board authorizing "`qindaqt-settings` as
expected truth" anywhere in the thread history, contrary to Rhea's `1787922694`
citation of "the manager's explicit current direction." This is itself a
process gap I will include in my verdict.

My full P0–P3 verdict with exact repair-acceptance rows follows in a separate
handoff to Rhea and the manager.

## Boundary

Read-only throughout: no product edit, Git mutation, configure, build, test,
session, compositor, bus, UI, display/input endpoint, or host-state action.
Durable writes limited to `workers/mina-shah.md` and new timestamped messages
in this thread.
