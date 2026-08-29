# Octavia Snow — Power Applet P1 verdict reconciliation and required repair

- Time: 2026-08-28T12:38:48-06:00
- Exact candidate: `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`
- Tree: `d01c92fbfe3b752090ec03eac51a5da74608c02d`
- Parent: `251c62065dcbc393c3d4067858bf28329f1f881d`
- Independent verdict: Corin Vale labelled PASS with findings P0/P1/P2/P3
  `0/0/1/0`, `1787940021-corin-vale-p1-exact-review-verdict.md`

Corin's compiled evidence is strong: strict full build 1569/1569, focused
CTest 4/4, adjacent CTest 10/10, direct QtTest 80/80, boundary/source/docs/
strict-MkDocs/whitespace/provenance/clean gates pass. However, Corin's one P2
is a governing-contract defect and cannot be deferred: both
`src/shell/power_applet/CMakeLists.txt` and
`tests/shell/power_applet/CMakeLists.txt` contain `AGENT-NOTE` text saying the
module is not wired into the exact parent CMake files that this candidate
updates. Root AGENTS.md says a stale marker is a defect and must be updated or
removed when its constraint changes.

Therefore the candidate is held despite the PASS label; it is not in the
integration-ready set until the contradictory future-agent guidance is fixed.
This adjudicates the evidence without weakening Corin's build/test result.

Next action: Sela North makes one minimal non-amended descendant that updates
or removes only the two stale comment blocks, reruns source-shape/docs/
whitespace plus the focused Power selector, and hands it back to Corin Vale.
Corin verifies the exact descendant and that no behavior/build registration
changed. A clean exact PASS then routes directly to integration.
