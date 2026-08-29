# Nash Calder — PB-1 midpoint + material findings

- Time: 2026-08-28T16:12:00-06:00 (2026-08-28T22:12:00Z)
- Worker: Nash Calder, Power PB-1 implementer (Z.AI coding plan, `glm-5.3`, reasoning high)
- Exact base: `f783f8389a563423e6e6bf2d98bd276748657a1e`
- Worktree: `/mnt/d/QindaQt/worktrees/power-pb1-nash` (branch `worker/power-pb1-nash`)

## Status

Implementation complete; Debug+Release focused evidence green. Preparing the
single candidate commit now.

- New modules: `src/services/power_service` (collaborator seams, coordinator
  with atomic LKG publication/epoch fencing, resident object, deterministic
  unavailable collaborators, activation package) and `src/services/power_client`
  (transport seam, QtDBus transport, exact-owner async client).
- New tests: 6 QtTest binaries + private-dbus residency/activation rows,
  installed package/consumer gate, source-policy boundary poison negatives.
- Evidence so far: dependency-light Debug full suite **245/245 passed**;
  power+brightness selection **14/14 Debug and 14/14 Release**; per-binary
  assertion totals 12+15+7+15+3+4 passed / 0 failed; `check-source-shape`
  1390 files 0 warnings; `validate-docs` 90 documents PASS; `mkdocs build
  --strict` PASS; `git diff --check` clean.

## Material fact 1 — pre-existing Release breakage (not mine)

`src/shell_customization_editor/src/keyboard_navigation.cpp` fails to compile
under GCC 16.1.1 `-O -Werror=maybe-uninitialized` (false positive in
`std::optional<DropTarget>` copy at keyboard_navigation.cpp:115). This
reproduces on the light-config Release build at my exact base and is
unrelated to Power paths. My Release gate therefore builds the focused
power/brightness target set (all 14/14 pass). Flagging for the Platform/Shell
managers; I will not touch that module.

## Material fact 2 — Qt foreign-owner `registerService` quirk

Qt reports a foreign-owned well-known name as a bare `registerService ==
false` with an empty `lastError()` (RequestName replies EXISTS as a result
code, not a D-Bus error). The NameExists-string check used by the Audio1 and
Display1 residents therefore cannot fire across connections. My Power1
resident instead asks `serviceOwner(name)` and classifies a non-empty foreign
owner as `NameAlreadyOwned`. Noted as a potential same-shape repair for the
audio/display residents' owners; not my scope in this slice.

## Plan

One non-amended candidate commit descendant of `f783f83`, then handoff in this
thread requesting a non-GLM exact-commit review.
