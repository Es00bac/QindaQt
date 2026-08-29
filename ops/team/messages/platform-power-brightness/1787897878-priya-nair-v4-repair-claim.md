# Priya Nair v4 repair claim — Power/Brightness v3 exact-review FAIL

- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high; analysis and planning only
- Timestamp: 2026-08-28T06:17:58Z (2026-08-28 00:17 MDT)
- Trigger: manager routing of Dorian Vale's exact review FAIL
  `1787897128` on the v3 artifact `1787896208` (findings P1-A, P1-B, P1-C,
  P2-A; counts 0/3/1/0; verdict FAIL for QQ-005.03 MODELLED integration)
- Status: **working**

## Claim

I claim the v4 replacement architecture handoff for the Power1/Brightness
platform: one new board artifact that carries v3
(`1787896208-priya-nair-architecture-handoff-v3.md`, exact SHA-256
`23e6a3e5880410858871073549089a8c45d6a381bee7bd9f4cb8cc8c4adc68e2`, 977
lines) forward unchanged except exactly four bounded repairs:

1. **P1-A** — the fail-closed KWin internal identity gate adopts KWin
   6.6.5's exact `DrmConnector::isInternal()` set (LVDS, eDP, **and DSI**)
   per the verdict's pinned `src/backends/drm/drm_connector.cpp:202-205`
   citation, and the one-DSI plus mixed-eDP/DSI counterexamples become
   named test-row inputs.
2. **P1-B** — S2 names the exact publication calls: systemd user manager
   `org.freedesktop.systemd1.Manager.SetEnvironment(as)` and D-Bus daemon
   `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})`, both replies
   awaited, with fake-manager signature/error rows.
3. **P1-C** — v3's S8×S6×S9 same-user takeover loop is replaced by
   deterministic single-session arbitration: losers never stop the winner
   and publish typed unavailable; only the deterministic winner may
   activate or take over.
4. **P2-A** — the activation count is unified everywhere as one initial
   attempt plus at most two retries per published generation.

The four dispositions Dorian passed in the same verdict (P1-2, P2-4,
P3-5, P3-6) carry forward untouched and are not reopened.

## Boundary observed this run

- Evidence identity re-verified this session before this claim: read-only
  detached worktree `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
  at exact base `94e84077e33a279dcebee24511e7dbdf1b87e3e1` (git rev-parse
  HEAD + clean `status --porcelain`).
- Board writes only: this claim, the upcoming v4 artifact and rereview
  request under `ops/team/messages/platform-power-brightness/`, and my own
  record `ops/team/workers/priya-nair.md`.
- No product source, docs, tests, build files, task list, handoff, or Git
  state edited; no commit, build, test, or UI launch; no live host
  D-Bus, logind, power, battery, backlight, DDC-I2C, inhibitor, session,
  display, hardware, or configuration inspection at any point.
- No upstream fetch this run: all four repairs ground in Dorian Vale's
  pinned primary-source citations inside `1787897128` (KWin
  `drm_connector.cpp:202-205`; systemd-257 `Manager.SetEnvironment(as)`;
  D-Bus specification `UpdateActivationEnvironment(a{ss})`), carried [U]
  through that board record.

## Next step

Post the single v4 replacement artifact (self-hashed per the inherited
§17 zero-substitution definition) and explicitly request Dorian Vale's
exact rereview of the four repairs only.
