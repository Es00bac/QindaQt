---
name: claude-container-rescue
role: Lane 2 implementer — stuck-container fix, maximized roll-up, touch/pen container gestures, optional standard desktop icons
provider: Anthropic Claude
model: claude-opus-5-5
status: working
feature: Containers never lock up; maximized containers roll up; touch/pen container gestures; optional standard desktop icons
worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/container-rescue
started_at: 2026-09-28T17:04:50Z
---

# claude-container-rescue

- Role: lane 2 of the 2026-09-28 personality round.
- Status: working — stuck-container fixes, maximized roll-up, modifier/pen/wheel chords and standard desktop icons written; building and testing on the laptop copy.
- Exact base: `47d2bb1f`.
- Branch: `fix/stuck-container-and-desktop-places`.
- Product authority: container interaction behavior in `src/compositor/kwin/` (placement, drag translation, pointer routing, shade/iconify, member policy, admission), `src/hybrid_input/`, `src/hybrid_gather/`, touch policy, input capabilities, the desktop-icons surface under `src/shell/desktop_surface/`, their tests and wiki pages. ADR-0282 reserved.

## Updates

- 2026-09-28T17:04:50Z: claimed lane 2; worktree created from 47d2bb1f.
- 2026-09-28T17:43:34Z: material findings — the owner's qinda-top session log shows 27 refused roll-ups of a maximized container; placement refused move/resize/roll-up while maximized, a lost gesture end left a baseline that refused every later Begin until the container dissolved, a member zoom hid the only title, and a member whose native move did not detach kept moving alone. Fixed each with regression tests (placement/rescue/maximize/title-wheel/member-policy suites); mutation proof: reverting the placement fixes fails 12 rows.
- 2026-09-28T17:43:34Z: scope additions from the manager (Outcomes C and D): maximized roll-up, Meta/Meta+Shift/Meta+right chords for mouse and pen, Meta+wheel roll-up. Implemented as pure hybrid_input policies (containerchords, wheelrollchord, tabletpointertranslator) plus KWin filter wiring; ADR-0282.

