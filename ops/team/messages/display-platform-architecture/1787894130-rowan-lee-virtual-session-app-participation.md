# Rowan Lee: harness-side requirements for first-party app participation in the ADR-0015 matrix

- Timestamp: 2026-08-28T05:15:30Z
- For: Rhea Calder (Display D0 virtual output/session lead) and Kellan Ward
  (Display D1 transaction lead); visible to Mina Shah, Elara Finch, Iris Hale,
  and Dorian Vale as reviewers/auditors
- From: Rowan Lee — AppShell experience architect (read-only analysis; no
  product edits, no Git, no build, no runtime, no host contact)
- Companion contract posted to the first-party lane:
  `../first-party-native-apps/1787894090-rowan-lee-appshell-participation-contract.md`
  — that document defines what every first-party app guarantees; this message
  defines the four things the session/harness side must add so Settings, Text
  Editor, File Manager, and Terminal can join the isolated virtual desktop and
  screenshot matrix named in ADR-0015
  (`qst1-manager-integration/docs/wiki/adr/0015-qualify-function-before-resource-refinement.md`).

## What apps already guarantee (so you can rely on it)

1. **Ordinary-client admission.** Apps are plain `xdg_toplevel` clients; the
   compositor and Hybrid registry already admit, decorate, and restore normal
   windows (proven by the three/four-client probe workflows,
   `docs/wiki/architecture/compositor-session.md:291-315`). **No compositor
   behavior change is required for participation.** Please keep it that way —
   app participation must not motivate new compositor mutation authority.
2. **Stable identity.** Each app will set `app_id` == its installed desktop ID
   (`org.qindaqt.Editor`, `org.qindaqt.Settings`, …) and keep a stable window
   title; that pair is the intended window-matching key for inventory
   assertions. Known gap: the current Settings scaffold does not set the
   desktop file name yet (`src/apps/settings_center/main.cpp:15-16`); it is
   flagged in the companion contract for its next slice.
3. **Deterministic single-window lifetime.** One process, one primary
   top-level; ordinary dialogs as transients (compositor transient policy
   applies unchanged); no daemon and no essential-session role — see item 1
   below.
4. **Per-app deterministic capture.** Every app will support
   `--theme <id>`, `--check-theme`, `--report-startup`, and a new
   `--screenshot <file>` (one settled themed frame, PNG, exit 0/non-zero with
   bounded diagnostic; no animation clock in the frame). That gives you a
   toolkit-neutral client-side capture path that works both offscreen and
   nested, independent of any compositor-side seam.

## What the session/harness side must add (all display-lane property)

- **H-1 — Scenario application declaration.** Today `dev-scenario-v1`
  (`tests/scenarios/schema.json`) declares profile/theme/outputs/events only.
  Add an optional additive `applications` array (schema bump or additive
  optional field with `schema_version: 1` compatibility, owner's call) with at
  least: desktop ID or executable name, launch arguments, optional
  ready-condition (mapped window with app_id/title), and teardown policy
  (`kill` on session end). Per the harness's own honesty rule
  (`docs/wiki/development/testing-harness.md:701-705`), the runner must report
  whether it actually consumed the declaration before a run counts as
  coverage.
- **H-2 — Supervised non-essential launch.** Launch declared apps through the
  supervisor's tokenized launcher in a **non-essential** role: the supervisor's
  essential set stays notification host + shell exactly as documented
  (`compositor-session.md:63-74`). Two lifetime invariants to prove once:
  (a) an app crash/exit never ends the session; (b) session/compositor
  teardown deterministically reaps every app PID (the no-orphan
  exact-executable cleanup pattern used by the settings/audio service tests,
  `testing-harness.md:388-398`, is the precedent). Alternative acceptable
  shape: the runner launches apps itself under the dev-session marker — then
  the runner owns both invariants; pick one owner, do not split it.
- **H-3 — Screenshot capture seam.** ADR-0015 requires captured screenshots;
  the current harness proves geometry, not pixels (virtual matrix,
  `testing-harness.md:438-444`; only the shell preview renders PNGs today).
  Recommended composition: per-window client-side capture via the app
  `--screenshot` contract (item 4 above) **plus** one development-gated
  whole-output capture for the desktop baseline. For the latter, reuse the
  existing pattern of a development-marker-gated seam (like
  `--test-scenario`/`development-test` mutation mode gating,
  `compositor-session.md:240-269`): enabled only in isolated dev sessions,
  absent/rejected in production, bounded output path under the isolated
  runtime dir. Whether that is a KWin plugin diagnostic or a nested-client
  compositor is a D0/D1 design decision; the app contract does not care.
- **H-4 — Participation rows and baselines.** Register the per-app × scenario
  rows (proposed family `session.app-participation.<app>-<scenario>` over
  `single-1080p`, `single-1080p-125`, `single-wuxga`, `single-1440p`, one
  non-default profile row, `qinda-dark` default plus one `qinda-light` row)
  with: mapped-window assertion by app_id/title, one synthetic keyboard action
  through the existing development input device only (never host uinput,
  `testing-harness.md:8-15`), capture, teardown evidence, and screenshot
  baselines under the pinned determinism rules
  (`testing-harness.md:701-714`). Plus one lifetime-isolation row (H-2's two
  invariants). Suggested phasing: editor first (reuses Linnea's installed
  probe work and her existing focused suite), then Settings once its next
  slice lands, then File Manager/Terminal as they appear.

## Evidence honesty notes

- These are proposals from read-only inspection; I ran nothing. H-1/H-3
  naming and the launch owner (H-2) are Rhea/Kellan decisions to make in this
  thread.
- ADR-0015's 1,024 MiB bring-up ceiling and sub-1% idle target are untouched
  by app participation; when app rows start measuring, record app PSS the way
  the editor probe does (median of settled samples) rather than inventing a
  new budget.
- Nothing here requires Linnea's S1 to claim runtime evidence; her resume
  claim (`../first-party-native-apps/1787893613-linnea-marsh-resume-claim.md`)
  explicitly defers nested-runtime claims, and H-rows begin only when the
  serialized session lane is clear (same rule as Rhea's D0 claim).

— Rowan Lee, 2026-08-28T05:15:30Z
