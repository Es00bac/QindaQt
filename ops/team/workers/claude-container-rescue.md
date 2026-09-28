---
name: claude-container-rescue
role: Lane 2 implementer — stuck-container fix, maximized roll-up, touch/pen container gestures, optional standard desktop icons
provider: Anthropic Claude
model: claude-opus-5-5
status: handoff
feature: Containers never lock up; maximized containers roll up; touch/pen container gestures; optional standard desktop icons
worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/container-rescue
started_at: 2026-09-28T17:04:50Z
---

# claude-container-rescue

- Role: lane 2 of the 2026-09-28 personality round.
- Status: handoff — touch long-press title pick-up added on fix/stuck-container-and-desktop-places (follow-up commit in the manager handoff); 16/16 focused ctest rows green on the laptop copy; not verified on the real touchscreen.
- Exact base: `47d2bb1f`.
- Branch: `fix/stuck-container-and-desktop-places`.
- Product authority: container interaction behavior in `src/compositor/kwin/` (placement, drag translation, pointer routing, shade/iconify, member policy, admission), `src/hybrid_input/`, `src/hybrid_gather/`, touch policy, input capabilities, the desktop-icons surface under `src/shell/desktop_surface/`, their tests and wiki pages. ADR-0282 reserved.

## Updates

- 2026-09-28T17:04:50Z: claimed lane 2; worktree created from 47d2bb1f.
- 2026-09-28T17:43:34Z: material findings — the owner's qinda-top session log shows 27 refused roll-ups of a maximized container; placement refused move/resize/roll-up while maximized, a lost gesture end left a baseline that refused every later Begin until the container dissolved, a member zoom hid the only title, and a member whose native move did not detach kept moving alone. Fixed each with regression tests (placement/rescue/maximize/title-wheel/member-policy suites); mutation proof: reverting the placement fixes fails 12 rows.
- 2026-09-28T17:43:34Z: scope additions from the manager (Outcomes C and D): maximized roll-up, Meta/Meta+Shift/Meta+right chords for mouse and pen, Meta+wheel roll-up. Implemented as pure hybrid_input policies (containerchords, wheelrollchord, tabletpointertranslator) plus KWin filter wiring; ADR-0282.
- 2026-09-28T18:17:52Z: handoff — commits 1d3191bf (containers + chords), d0e06058 (standard desktop icons), 6a2dce86 (Trash icon refresh + harness); laptop-build.sh ctest: 16/16 compositor/hybrid-input/settings rows and 14/14 desktop-surface rows passed; qindaqt_compositor and qindaqt-shell built; ./tools/validate-docs green; no new source-shape error. Remaining: live-session checks (real Wacom barrel/tip, touchpad Meta+scroll, KWin resize following a pen, zoomed-member wheel roll-up); touch long-press pick-up of independent windows' native titles not implemented.
- 2026-09-28T19:37:22Z: claimed the manager's follow-up (touch long-press pick-up of native titles now that qinda-top has a touchscreen); read kwin-6.6.6 DecorationEventFilter/TouchInputRedirection to take over without leaving KWin's decoration touch-press id set.
- 2026-09-28T19:37:22Z: handoff — TouchTitlePickup policy + KWin touch adapter wiring; ctest 16/16 (incl. hybrid.input-touch-title-pickup, compositor.hybrid-chrome-touch-policy, compositor.kwin-input-adapter); qindaqt_compositor built; validate-docs green.

