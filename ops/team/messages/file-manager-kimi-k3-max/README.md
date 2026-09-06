# Shared file-manager coordination thread

This is the shared communication location for the user's separate file-manager
worker and the active QindaQt Program Manager. Read and write this exact path
in the main checkout, rather than a worktree-local copy of this thread:

`/home/cabewse/work_SPaC3/container-wm/ops/team/messages/file-manager-kimi-k3-max/`

The user explicitly requested shared-file communication. This shared thread is
for coordination only; product implementation remains in an isolated branch
and worktree under the repository's AGENTS.md workflow.

## Messages

Write a new timestamped Markdown file for each claim, material finding, shared
boundary request, review request, or handoff. Use UTC timestamps such as
`20260906T214500Z-worker-claim.md` and `20260906T214600Z-manager-reply.md`.
Never rewrite another person's message. Read newer replies before changing a
shared build file, public interface, service contract, or another owner's path.

First claim must give:

- Worker identity and actual provider/model evidence, if available.
- PID or other verifiable active-process evidence; do not equate an assignment
  with a live process.
- Absolute worktree, branch, and exact base commit.
- Owned implementation/test/documentation paths and intended outcome.
- Any existing uncommitted work and any shared boundary needed.

The worker owns file-manager usability, SMB/network-filesystem browsing, and
filesystem-mounting integration, plus their focused tests and primary docs.
The Program Manager's current workers own Display Settings, shell/global menu,
container geometry/chrome, task switching, portal input, and Gabbee validation.
Coordinate changes to shared registries, CMake files, Network service APIs,
polkit/udisks integration, session defaults, and common controls before editing.
There is no blanket reservation of unrelated Network status functionality.

## Handoff and integration

Name the exact candidate commit, changed paths, executable test commands and
actual results, bounded remaining caveats, and requested next action. The
Program Manager will arrange independent exact-commit review and integrate
accepted work; do not merge or push to main yourself. Preserve unfinished work
and never clean up or reset someone else's changes.

Maintain your own worker record and durable handoffs under your worktree's
`ops/team/` according to AGENTS.md; include their absolute paths in this shared
thread. The shared inbox bridges independently launched sessions. It is not an
automatic event bus: each active agent must read it at work boundaries and
respond in a new file.
