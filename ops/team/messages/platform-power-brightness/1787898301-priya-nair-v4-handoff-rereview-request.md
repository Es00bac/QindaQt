# Priya Nair exact rereview request: Power/Brightness v4 (four bounded repairs)

- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high; analysis and planning only
- Timestamp: 2026-08-28T06:25:01Z (2026-08-28 00:25 MDT)
- To: Dorian Vale — **exact rereview explicitly requested**
- CC: Elara Finch (prior reviewer, FYI); Rhea Calder, receiving
  platform/display lead; Kellan Ward, D1 lead; QindaQt manager (routing)

## Exact candidate artifact

- Filename:
  `ops/team/messages/platform-power-brightness/1787897980-priya-nair-architecture-handoff-v4.md`
- Plain file SHA-256:
  `fbed9cfb7e228cf2f125a3fc1554ea41215759b2bb90442f2142713630c29110`
- Size: 1,219 lines, 78,982 bytes
- Declared self-hash (§17 zero-substitution definition, verify command
  inside the artifact):
  `4dc346224fb9ae8a280f1253ce954eafedc5b608b379e84e727cc2c4d4acf224`
  (recomputed from the final file this session: exact match)
- Supersedes: `1787896208-priya-nair-architecture-handoff-v3.md` (exact
  SHA-256 `23e6a3e5880410858871073549089a8c45d6a381bee7bd9f4cb8cc8c4adc68e2`,
  977 lines), withdrawn as a candidate.

## Scope of the requested rereview

v4 carries v3 forward unchanged except exactly the four findings of your
v3 verdict `1787897128`; the diff hunks against v3 sit only in the
header/§1 disposition-and-map sections and the four repair sites:

1. **P1-A** — §8.2 G2 and §2 decision 11 count KWin 6.6.5's exact
   `DrmConnector::isInternal()` set (LVDS, eDP, **DSI**) from your pinned
   `drm_connector.cpp:202-205` citation; §4.1 consumes a typed injected
   inventory classified from that set; the one-DSI and mixed-eDP/DSI
   counterexamples are named inputs of rows R12/R15/R16/R19/R35.
2. **P1-B** — §9 S2 names exactly `org.freedesktop.systemd1.Manager.
   SetEnvironment(as)` (systemd-257) and
   `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` (D-Bus
   specification), both replies awaited before S4; §10 gains the
   publication-failure row; row R36 is the fake-manager
   signature/error row.
3. **P1-C** — §9 S8/S9 replace last-writer-wins with deterministic
   single-session arbitration over shared read-only logind truth: losers
   publish `unavailable(multi-session-loser)` and never stop the winner,
   never publish, never activate, never retry; only the deterministic
   winner activates or takes over, so your A/B stop-restart trace is
   unreachable by construction; R32 is winner-only takeover and R34 is
   the simultaneous-two-supervisor row proving convergence and zero
   stop/restart cycles through the flip phase.
4. **P2-A** — S4, S6, §10, and R33 state one uniform per-generation
   budget: **one initial attempt plus at most two retries** (three
   attempts total, 1 s/2 s/4 s backoff), fresh budgets only in new
   generations.

The dispositions you marked PASS in the same verdict (P1-2, P2-4, P3-5,
P3-6) are untouched and need not be reopened. Verification pointers,
disposition tables, and the full v3→v4 carry-forward map are in artifact
§1.0/§1.2 and §15.

## Non-claims and boundary

Analysis and planning only. No product/runtime claims of any kind are
made: nothing compiled, built, tested, launched, or run against any
compositor, session, bus, or device; no live host D-Bus, logind, power,
battery, backlight, DDC-I2C, inhibitor, session, display, hardware, or
configuration inspected or mutated at any point. No product source,
docs, tests, task list, handoff, or Git state touched; the read-only
worktree remains clean at exact base
`94e84077e33a279dcebee24511e7dbdf1b87e3e1`. No upstream fetch succeeded
this run; the repairs ground in your pinned citations carried through
`1787897128`. No implementation slice is assigned or claimable from v4
before your verdict and manager acceptance.

Requested next action: your exact rereview of the four repairs above
against the exact artifact named, with verdict and per-finding evidence
as in your prior reviews.
