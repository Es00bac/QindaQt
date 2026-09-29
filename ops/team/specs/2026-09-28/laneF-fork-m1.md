# Lane F (Opus) — Plasma-free milestone M1: the `qindaqt-kwin` fork's identity

Worker name `claude-qindaqt-kwin`, voice seat `claude-helper-one`, speech name "KWin fork".
The plan is `docs/plans/2026-09-28-plasma-free-qindaqt.md` on hub branch `plan/plasma-free`
(commit cc582feb, including the "Owner decisions" section). Execute milestone **M1 (F1–F8, plus the
new native "kill a root-owned hung window" slice the owner asked for)** in the plan's order, one
reviewable commit per slice.

- Create the fork repository as the plan says (history starting from KWin's `v6.6.6`; bare hub
  `qinda:~/git/qindaqt-kwin.git`, working checkout `qinda:~/work_SPaC3/qindaqt-kwin`), with the two
  QindaQt patches (container-wm `compositor/patches/0001`, `0002`) as its first commits.
- Apply the identity renames slice by slice so that `gui-wm/qindaqt-kwin` installs beside an
  unmodified `kde-plasma/kwin` with zero file collisions (verify with a staged install and a file-list
  diff against `qlist -e kde-plasma/kwin`), keep compatibility names only where the plan says.
- container-wm changes (plugin install paths, session launcher, docs/ADRs the plan lists) go on a
  branch `feature/qindaqt-kwin` in a container-wm worktree off `hub/main`; the overlay ebuild
  `gui-wm/qindaqt-kwin` in a QindaGentoo worktree/branch. Don't switch the live session or the
  delivery list; the manager integrates and delivers.
- Build on qinda with `-j12 -l24` (another lane builds too), run the focused checks the plan names,
  push each repo's branch, and report: repos/branches/commits, the collision-diff result, what each
  slice verified, and anything that must be decided by the owner. Speak progress per the round brief.
  Keep token use low: read what you need, no narration.
