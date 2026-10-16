## Environment and workflow (read carefully)
You run on the laptop `qinda-top`. Edit source in a laptop-local git worktree; build and test on `qinda`
(24 cores) over ssh. The laptop has ~13 GB RAM under pressure: never build on the laptop.

container-wm:
- Create your worktree from the hub commit (the laptop repo's `origin` IS the internal hub here):
  `git -C ~/work_space/container-wm fetch origin`
  `git -C ~/work_space/container-wm worktree add -b <BRANCH> ~/work_space/container-wm.worktrees/<WT> <BASE>`
  (Do not touch ~/work_space/container-wm itself: it is the owner's checkout on another branch.)
- Sync to qinda for building (plain source tree, no .git):
  `rsync -a --delete --exclude=.git --exclude=/build/ --exclude=/.cache/ ~/work_space/container-wm.worktrees/<WT>/ qinda:work_SPaC3/container-wm.worktrees/<WT>-src/`
- Configure once on qinda:
  `ssh qinda 'cd ~/work_SPaC3/container-wm.worktrees/<WT>-src && cmake --preset release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache'`
- Build only what you need, under the shared lock:
  `ssh qinda 'cd ~/work_SPaC3/container-wm.worktrees/<WT>-src && nice -n 10 ionice -c3 flock ~/.cache/qindaqt-program-build.lock cmake --build build/release --target <targets> -- -j20 -l24'`
  then `ctest --test-dir build/release -R '<regex>' --output-on-failure`.
- Commit on your laptop branch, then push ONLY to the internal hub: `git -C <laptop worktree> push origin <BRANCH>`
  (laptop `origin` = ssh://desktop~/git/container-wm.git = hub). Never push to github.com.
- Specs: `ops/team/specs/2026-10-16-stability/` inside your worktree (worker-brief.md, findings.md, your lane spec).

qindaqt-kwin (L4 only):
- `git -C ~/git/qindaqt-kwin.git fetch origin` then
  `git -C ~/git/qindaqt-kwin.git worktree add -b <BRANCH> ~/work_space/qindaqt-kwin.worktrees/<WT> 24d0c6a6fe`
  (bare mirror; `origin` = ssh://qinda~/git/qindaqt-kwin.git = hub). Sync with rsync to
  `qinda:work_SPaC3/qindaqt-kwin.worktrees/<WT>-src/` and build there per qindaqt/README.md under the same lock.

Board (shared container-wm checkout on qinda, manager's tree — only these files):
- `ssh qinda 'cat > ~/work_SPaC3/container-wm/ops/team/workers/<WORKER>.md'` with `- Status: working — <outcome>`,
  `## Updates` and ISO-8601 bullets; update at claim, midpoint/finding, handoff (set Status: handed-off).
- Claim/handoff notes: new files `ops/team/messages/<UTC timestamp>-<WORKER>-<claim|handoff>.md` there.

Live system: you may READ anything on the laptop (journals, /proc, busctl introspect/get-property) to verify
behavior, but never restart, signal, lock, suspend or reconfigure anything, and never install packages.
