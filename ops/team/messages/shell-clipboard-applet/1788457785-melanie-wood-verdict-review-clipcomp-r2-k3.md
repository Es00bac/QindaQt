# Exact-candidate recheck — Clipboard applet C2 consent-denial negative controls

- Reviewer persona: **Melanie Wood**, independent shell reviewer (slug `melanie-wood`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `fef92227bb2edd847d1c88373855d90aca0f042d` (verified `git rev-parse HEAD`; `git status --porcelain` empty before and after)
- Tree SHA: `dae0482a4155b610ec09d405414089ba8b9bef76`
- Parent SHA: `98ecfc53538d4c0b074044399fdcce11684395bb`
- Base SHA: `a069842659e3e96dd93d0f27a66049d9d3ff03c8` (prior rejected candidate; reviewed `git diff a069842..fef9222`)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-composition-k3-review` (detached, read-only; no product path touched)
- Build root `<ROOT>`: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipcomp-k3` (incremental rebuild)

## Scope

This is the funded single recheck of the P2-1 repair. The diff is additive:
four registered `qindaqt.clipboard-applet-consent-{missing,malformed,inherited-default,ownerless}`
rows driving `withholdsDeniedConsentUntilFencedEmptySnapshot` in
`tests/shell/clipboard_applet/tst_clipboard_applet_composition_private_bus.cpp`,
a parameterized `FakeSettingsTransport::setReply`, a generalized `snapshot()`
helper, the original private-bus row pinned to its original function name in
CTest, and matching wiki updates (`clipboard-applet.md` test matrix,
`testing-harness.md`). No production source changed; no JSON changed.

## Findings ledger

### P0 — none

All rows are private-bus only; every ctest run used
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`.
No nested (`tests/session`) row was run.

### P1 — none

The prior P2-1 gap is closed. Each denial row now:

1. Starts the real composition against a Clipboard1 service exposing content
   at generation 41 (history enabled, one entry) with one denied consent
   variant, and requires `unavailable` /
   `unavailable: clipboard-consent-{unavailable|denied}` with `entryCount == 0`
   — existing content is withheld while the service still reports it.
2. Normalizes to an explicit `false` user-override at revision 2 and requires
   `unavailable: clipboard-consent-denied`, still 0 entries.
3. Publishes the generation-advanced (42) empty purge snapshot with
   `historyEnabled=false` and requires the `disabled` phase with exact reason
   — Disabled is exposed only from the fenced empty snapshot, never forged at
   the old generation.

Row/kind mapping is exact: `missing` (key absent, client Degraded →
`clipboard-consent-unavailable`), `malformed` (`"true"` string from
`user-overrides` → `clipboard-consent-denied`), `inherited-default` (`true`
from `system-defaults` → `clipboard-consent-denied`), `ownerless` (no
Settings1 owner, client Authenticating → `clipboard-consent-unavailable`).
This matches the wiki denial enumeration verbatim.

### P2 — none

The rows are real controls, not vacuous. Independent negative control (my own,
not the implementer's): scratch copy of
`src/shell/runtime/clipboardappletcomposition.cpp` under `<ROOT>/scratch/negctl`
with `explicitHistoryConsent` neutered to `return true;`, compiled with the
exact ninja flags of the real target and linked into a scratch binary
(scratch only; build-tree binary untouched). Running
`withholdsDeniedConsentUntilFencedEmptySnapshot:<kind>` for all four kinds:
exit 1 each, one `FAIL!` each (the initial phase-reason assertion — the
neutered composition reaches ready/content instead of withholding). Removing
the denial path fails every row. The committed binary passes the same rows
(see ctest below).

### P3 — none

## Commands and results

1. Configure Debug and Release with the common-contract recipe
   (`qindaqt-665-initial-cache.cmake`, all prescribed flags) → exit 0 each.
2. `cmake --build <ROOT>/{debug,release} --parallel 3 --target
   qindaqt_clipboard_applet_composition_private_bus_tests` → exit 0 each.
3. Debug: `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
   ctest --test-dir <ROOT>/debug -R '^qindaqt\.(clipboard-applet-|applet|shell-runtime-)'
   --output-on-failure --no-tests=error` → exit 0, **29/29** (includes all four
   new consent rows, each Passed).
4. Release: same selector → exit 0, **29/29**.
5. Independent scratch negative control (P2 section) → all four rows fail
   (exit 1, 1 FAIL! each) with the denial gate removed.
6. `./tools/validate-docs` → exit 0 (135 documents + navigation).
7. `mkdocs build --strict --site-dir <ROOT>/site` (docs venv) → exit 0.
8. `./tools/check-source-shape` → exit 0 (pre-existing threshold warnings
   only; `shellruntimeapplication.cpp` still 499 non-blank lines, unchanged).
9. `git diff --check a069842..fef9222` → exit 0.
10. `python3 -m json.tool` on changed JSON → not applicable; the diff changes
    no JSON.
11. Post-work `git rev-parse HEAD` = candidate; `git status --porcelain`
    empty.

## Verdict

The repair is minimal, correctly targeted, and proven: all four denial kinds
are covered by registered rows that withhold content immediately and gate
Disabled on the generation-fenced empty snapshot, and my independent negative
control confirms each row fails if the denial path is removed. Nothing else
regressed (29/29 in both profiles, all static gates green).

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
