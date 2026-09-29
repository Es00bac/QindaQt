# Common worker brief: QindaQt program lanes (2026-09-28). Read it in full before starting.

The manager (Claude or Codex, whoever holds `docs/HANDOFF.md` "RESUME HERE") runs a small number of feature lanes. You own ONE lane
(your assignment follows this brief). Other lanes run concurrently in other paths.

## Build and test rules (the owner dropped the scoring game on 2026-09-28)
Think first, then build. Build only your targets in your own worktree on qinda with
`cmake --build build/dev --target <targets> -- -j8 -l24` (configure once with `cmake --preset dev`),
run your focused tests with ctest, and fix what fails. No full test suite; the manager runs that once.
Be economical with tokens: read only what you need, and don't narrate.

## Where the work happens: on the workstation `qinda`, over SSH
- Claude workers run on the laptop (qinda-top); Codex workers run on qinda and skip the `ssh qinda`. ALL source edits, builds, tests and commits happen on `qinda` via
  `ssh qinda '...'` (non-interactive key auth works). Do not edit `~/work_space/*` on the laptop.
- Hubs on qinda: `~/git/container-wm.git` and `~/git/QindaThemes.git`; start from the current hub `main`.
  Working checkouts `~/work_SPaC3/container-wm` and `~/work_SPaC3/QindaThemes` belong to the manager:
  never edit, commit or check out anything in them. Create your own worktree as your assignment says.
- Remote shell is zsh: quote globs, avoid bare `====` lines, use `${var}` before a colon. For multi-line
  edits prefer `ssh qinda 'python3 - <<"EOF" ... EOF'` or write a local file in your scratch area under
  your own scratch directory and `ssh qinda 'cat > path' < localfile`, then re-read the remote file to verify.
- Never install anything system-wide, never run emerge/sudo, never restart services or the live session.

## Repository rules (read `AGENTS.md` and the wiki pages for the module you change)
- Modular: one responsibility per module; policy, persistence, platform and presentation separate. No
  hand-written file over 500 non-blank lines without decomposition; never over 600 (check with
  `tools/check-source-shape` in container-wm).
- Comments are for a future agent: intent, invariants, traps. Use `AGENT-NOTE:` / `AGENT-GUARD:` /
  `AGENT-CONTRACT:` markers where a constraint is non-local. Match surrounding style.
- Documentation is part of the change: update affected `docs/wiki/` pages; add an ADR for a durable
  cross-cutting choice (use the ADR number your assignment reserves for you — do not pick another); run
  `./tools/validate-docs`.
- Tests: focused tests for every changed module (`ctest -R ... --output-on-failure`), QML tests run with
  QT_FATAL_WARNINGS. Model mutations need invariant tests. No information by colour alone; keyboard and
  accessible names for new controls. Unknown values show as absent, never as zero.
- The owner waived separate reviewer agents: YOUR tests are the verification, so test thoroughly and
  report honestly. Don't claim anything you did not directly observe.
- Worker record: create `ops/team/workers/<your-worker-name>.md` in YOUR worktree with
  `- Status: working — ...`, a literal `## Updates` heading and ISO-8601 bullets; update at claim,
  midpoint and handoff (`- Status: handoff — ...`). (container-wm lanes only; QindaThemes has no board.)

## Spoken updates (required)
The owner is your manager and does not read code. Speak progress yourself with ElevenLabs:
`cd ~/.cache/claude-voices/<your-seat> && ~/.local/bin/codex-say "This is the <lane name> worker. ..."`
run ON THE LAPTOP (not over ssh; codex-say is local). Luna lanes that run on qinda speak with
`ssh qinda-top 'cd ~/.cache/claude-voices/<seat> && ~/.local/bin/codex-say "..."'`.
Speak at start, on every real discovery or decision (what you found, what you chose and why, what you
rejected), when blocked, and at least every 2–3 minutes of sustained work, 15–40 seconds each, plain
language, no file names, code or raw output, never secrets. Run it in the background (`... &`) so it
does not block you. If the helper fails, continue and mention it once in your final report.

## Handoff (your final answer to the manager)
Commit on your branch (imperative subjects; body says what contract landed and which gates passed; say
"Independent review waived by the owner"; end each message with
your own model attribution line), push the branch to the hub,
then report: 1) exact full commit SHA(s) and worktree path; 2) changed files (summary); 3) every
build/test command with pass/fail counts and exit status; 4) ADR number used; 5) remaining caveats
(what needs a live session to confirm), and anything you could not do. If blocked, stop and say why.
