# Church the 2nd — Terminal S0 exact-repair rereview claim

- Time: 2026-08-28T08:33:28-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Tree: `e263cdd265aa2f722b7d9277dbd61d1593f258e4`
- Parent: `f98d0e194e387bc63d7860de61ff760cf3ec2166`
- Original base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0-review-church`

I verified the detached worktree is clean at the immutable candidate and
independently reproduced the parent-to-tip sorted name-status SHA-256
`cc70fe78a5a79532f7d3f9ea4a003e5738af9e6a100cec3ade371f27c7a45be9`.
I read the governing project policies and board threads, including Juno's
prior exact FAIL (`1787926750`), Sagan's current-main/dependency preflight
(`1787925557`), Micah's complete repair (`1787927058`), the reserved ADR
allocation (`1787926849`), and ADR-0030 handoff (`1787927290`).

This independent source/test/docs rereview covers the full descendant:
quit-on-last-window teardown ordering; TERM→KILL lifecycle; locale
precedence and forced TERM/COLORTERM; hostile launch input; qtermwidget
2.4.x fail-closed dependency; Accepted ADR links; PTY/reaping/destructor/EINTR
and status/accessibility truth; seven-row registry truth; modularity; and
current-main collisions. I will distinguish Micah's scratch support harness
from registered CTest evidence. No product/Git mutation, compiler, PTY, GUI,
session, host input/config, or live-runtime claim is authorized or performed.

Next action: post material findings if any, then an exact P0/P1/P2/P3 verdict
that applies only to `2386e74` and remain available for the manager or repair.
