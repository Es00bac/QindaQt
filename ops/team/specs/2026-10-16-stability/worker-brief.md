# Stability round 2026-10-16 — shared worker brief

Owner request (2026-10-16): "Fix the problems. If there is a common issue linking any of these problems, which
fixing would fix other problems, then, obviously, fix that to fix the others. I need a stable desktop."
"Look at how other Desktop environments solve the same problems if that helps at any point. Don't pull in
dependencies from other desktops, clean Qinda implementations of things only."

The investigation behind this round is `ops/team/specs/2026-10-16-stability/findings.md`. Read the items your
lane owns before you start, then verify them against the code yourself. Treat file:line references as starting
points, because they came from a read-only audit.

## Common root causes (fix the cause, not each symptom)
1. **Bus discipline.** Components poll the session bus with periodic *synchronous* calls, broadcast high-rate
   signals to everyone, and treat one slow reply as permanent loss. Replace polling with event-driven watchers
   (QDBusServiceWatcher, pidfd, socket HUP), use async calls with short timeouts off any UI/compositor main
   thread, emit change signals only on real change, and coalesce bursts.
2. **Fail-closed-forever lifecycles.** After one failure, several components never retry or re-attach. Every
   attachment, child process and subscription must recover: bounded exponential backoff (for example 0.25 s
   doubling to 30 s), automatic re-attach when the peer returns, and a log line that says why.
3. **Blocking work on the compositor main thread.** No filesystem walks, process I/O or synchronous D-Bus round
   trips in per-frame or per-event compositor paths. Cache, precompute or move them off the main thread.

## Rules
- Other desktops (GNOME: mutter, gnome-shell, gnome-settings-daemon, gnome-session; KDE: KWin, KScreenLocker,
  PowerDevil, libtaskmanager) may be studied for design. Do not add any of their libraries, services or
  packages as dependencies. Write clean QindaQt code that follows QindaQt's ADRs and module boundaries.
- Read the repository's `AGENTS.md`, the wiki pages for your modules, and `docs/wiki/development/coding-practices.md`.
  Keep each module's single responsibility. Files must stay under 500 non-blank lines, or include the
  decomposition the policy requires.
- Work only in your own git worktree on qinda. Branch from the exact base commit given in your lane spec, and
  never edit the shared checkout `~/work_SPaC3/<repo>` beyond board files. Edit only the paths your lane owns.
  If you need a change elsewhere, write it up in your handoff instead of making it.
- Nothing live may be touched. On the laptop, don't restart or signal any service and don't install anything.
  Don't build binpkgs or emerge. The manager packages and installs once at the end.
- qinda's disk has very little free space. Build only the targets you need, with the lane's build directory
  and flags. Run every build under `nice -n 10 ionice -c3 flock ~/.cache/qindaqt-program-build.lock`, with
  `-j12 -l20`. Leave the qinda game server process alone.
- Think the code through before building. Per-file syntax checks are allowed. Builds and targeted tests are not
  rationed, but each one should have a purpose.
- No separate reviewer is assigned (the owner waived independent review). Your own tests are the only
  verification, so test thoroughly and report results honestly, including anything you could not run.
- Write tests for every behavior change. Every new retry/backoff and coalescing rule needs a deterministic test.
  Use an injectable clock or timer, not sleeps.
- Documentation is part of the change. Update the wiki pages your behavior touches, and add an ADR when a
  contract changes. Run `mkdocs build --strict` and the repository link checker before handoff when you touch
  docs (container-wm only).
- Comments follow the repository's AGENT-NOTE / AGENT-GUARD / AGENT-CONTRACT conventions.
- Commits: subject is imperative and names the outcome. The body says which contracts changed, which tests ran
  (with counts), and ends with "Independent review waived by the owner." In the fork, follow its AGENTS.md:
  `qindaqt:` subject prefix and a `QindaQt-ADR:` trailer.

## Git remotes (hard rule)
- Never push to any github.com remote. In `~/work_SPaC3/container-wm` (and worktrees made from it) `origin` is
  the PUBLIC GitHub repo; the internal hub is the remote named `hub`. In qindaqt-kwin `origin` is the hub and
  `github` is public. Push your branch only with an explicit internal remote, e.g. `git push hub <branch>`
  (container-wm) or `git push origin <branch>` (qindaqt-kwin), after checking `git remote -v`.

## Board and voice
- When you claim the work, create `ops/team/workers/<your-worker-name>.md` in the shared container-wm checkout
  on qinda. It needs `- Status: working — <outcome>` and a `## Updates` heading with ISO-8601 bullets. Post
  claim and handoff notes as new files under `ops/team/messages/`. Keep them short.
- Spoken updates: from your voice seat (`cd ~/.cache/claude-voices/<seat> && ~/.local/bin/codex-say --stdin
  <<'SAY' ... SAY`), at your start, on a major finding or decision, and at handoff. Speak at most once per
  10 minutes and keep each update under 30 seconds. Use plain language with no file names or raw output, and
  begin with "This is the <lane> worker." Voice commands run on the laptop (qinda-top), not on qinda.

## Handoff (your final message)
Report:
- exact commit(s) and branch;
- changed paths;
- tests, with commands, exit status and counts;
- docs checks;
- any remaining caveats, with exact descriptions;
- the measured before/after numbers where your lane has them.
Leave your build directory in place unless your spec says otherwise.
