# Malik Hart — post-crash Platform Services integration ledger

- Time: 2026-08-28T18:40:05Z
- Manager: Malik Hart, Platform Services Workgroup Manager
- Evidence boundary: read-only inspection of the shared board, Git refs/objects,
  worker worktrees, and the current manager integration worktree. No product,
  worker, integration, feature, task-list, or handoff file was changed.
- Liveness rule: a worker profile saying `working` is recorded as a declaration,
  not proof that the provider process survived the crash.

## Ordered integration/review ledger

1. **Clipboard C0 — accepted; already carried by the manager integration line.**
   Exact candidate `aad0ff2d6b35d2223a61f5528964614cba03fcc9`, tree
   `34e0da73d62b19b2cc594083957c2574ba601f87`, parent `08d4352...`, is clean
   and independently PASS `0/0/0/0` from Hopper the 2nd. Debug and Release
   Clipboard selectors passed 4/4 each; hostile prefix/aggregate reproduction,
   strict MkDocs, docs, source-shape, and whitespace passed. Its complete
   lineage is already replayed on `manager/appearance-settings-s0-integration`
   as `2453039` → `6907f1f` → `758540c` → `7da293d`; do not cherry-pick
   `aad0ff2` again. Safest next action: finish the fresh combined-tree QA at
   manager HEAD `631fa4404fdee1d22a3bfe7ed12b436ea9b6b2b1`, then let only the Program
   Manager publish/update the product ledger. C1 transport, lock authority,
   Wayland adaptation, packaging, and UI remain later.

2. **Power status/brightness and applet — PB-0 public; applet accepted and
   already carried by the manager integration line.** PB-0 exact accepted
   content is integrated at `cbec6fb42216e5bcc3283004473be7f5f6ccda66`.
   The applet repair candidate
   `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`, tree `d01c92fb...`, parent
   `251c620...`, passed Corin Vale's cross-provider review with findings
   `0/0/1/0`: full strict build 1569/1569, focused 4/4, adjacent 10/10,
   direct QtTest 80/80, strict docs/source-shape/whitespace all PASS. Candidate
   content is already replayed on the manager line as `7fa01ae` → `631fa44`;
   do not cherry-pick it again. Before publication, fix Corin's non-blocking but
   real stale `AGENT-NOTE` text in both Power applet CMakeLists; the stale text
   remains present at manager HEAD. Complete combined-tree QA afterward.
   Resident Power1 service/client/upstream adapters remain absent; applet P1 is
   presentation-only and must not advance that service claim.

3. **Audio1 — integrated; Audio applet repair is preserved but not a
   candidate.** The bounded resident Audio1 stack is already integrated and
   QQ-005.01 is `EXECUTABLE`; physical device/UI/resource qualification remains.
   Audio applet exact `262a8493fe5f15991675b6a0f5ef575d4854d19b` failed Astra
   Quill's rereview `0/2/0/0`. Rune Mercer's worktree preserves the two-file
   unstaged repair (10 insertions/4 deletions) for the missing QObject-parent
   constructor and sequence-point failures. Safest next action: Rune commits a
   non-amended descendant after strict build/tests, then Astra rereviews that
   exact descendant. No integration before PASS and the still-required shell,
   manifest, capability-policy, QML-module, and composition seams are proven.

4. **Bluetooth B0 — rejected base plus large preserved repair; no candidate.**
   Lovelace the 2nd rejected exact `e19d094c792d132d3d65129056281ca556415c0f`
   at `0/9/5/3`. The isolated worktree preserves Samira Cole's staged 33-path
   repair (`+1586/-404`) plus Cassia Rowan's 12-path unstaged continuation
   (`+81/-46`) and untracked `Testing/`. No descendant commit or post-repair
   build/test result exists. Safest next action: Cassia resumes/finishes without
   losing either index layer, runs the promised Debug/Release, private-bus,
   installed-consumer, hostile-wire, docs/source-shape gates, commits one
   non-amended descendant crediting Samira, then Lovelace rereviews that exact
   commit. Do not review or integrate the dirty worktree.

5. **Display D3 — partial recovery implementation and hostile tests preserved;
   no candidate.** Base `146fc48358c2659436dec4fc6b6062d23c5ee746`, branch
   `worker/display-d3-kimi-nyra`. Nyra Sol's Kimi process ended on a verified
   weekly-limit 403. Pavel Shore owns recovery; Tara Wells's five hostile test
   binaries are present under `tests/services/display_client/**`. The tree is
   uncommitted: additive source/test registries, complete transport/client
   declarations, missing client implementation/coordinator, plus local fallback
   board artifacts. No build directory or test result exists. Safest next
   action: Pavel completes the public-source side and header-name contract;
   Tara validates her exclusive tests once linkable; Pavel hands off one clean
   commit for a non-Claude exact review. Do not count Tara as an independent
   reviewer because she authored the hostile tests.

6. **Network N0 — uncommitted pure slice; no candidate or test evidence.** Base
   `146fc483...`, branch `worker/network-n0-glm-veda`; untracked protocol,
   model, client, and matching tests plus six additive registry lines are
   preserved. Veda Park's shared record declares work, but post-crash liveness
   is not independently proven. Safest next action: resume the exact Veda lane,
   finish docs/install-consumer and strict gates, commit once clean, then assign
   a non-GLM exact reviewer. N0 must remain fake-transport-only with no
   NetworkManager/D-Bus/host-radio/secret/UI authority.

7. **Font F0 — uncommitted pure slice; no candidate or test evidence.** Base
   `146fc483...`, branch `worker/font-f0-kimi-oria`; source/tests and two
   additive registry lines are preserved. Faye Lin's shared takeover claim is
   durable; the worktree also contains a local fallback copy of her profile.
   Safest next action: resume Faye, finish docs and strict Debug/Release/hostile
   tests, commit one clean candidate, then assign a non-Gemini exact reviewer.
   No live fontconfig/filesystem mutation or GUI claim is authorized.

8. **Display Color C0 — orphaned partial recovered from a worktree-only board;
   no shared liveness and no candidate.** Worktree
   `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c0-gemini-solene`,
   branch `worker/display-color-c0-gemini-solene`, base `146fc483...`, contains
   uncommitted `src/services/display_color_model/**`, matching hostile tests and
   installed consumer, and two additive registry lines. Solene Ward's claim and
   profile exist only under that worktree's fallback `ops/team/`, not on the
   shared board; no build or test evidence exists. Safest next action: Program
   Manager reconnects the exact Solene persona/process or appoints a named
   recovery owner who first audits every preserved byte, then finishes one
   clean candidate for non-Gemini exact review. The partial must not be deleted,
   reset, or treated as live/integrable.

## Integration boundary

`origin/main` is `146fc48358c2659436dec4fc6b6062d23c5ee746`. The current shared
checkout `main` is older at `c4982697858c083828bd406f1aa56c4e942bcc10`; workers
must use the Program Manager's declared integration base, not infer it from the
shared checkout. The active private manager line at `631fa440...` already carries
the accepted Clipboard and Power applet content among other outcomes and is
under combined QA. No other platform lane currently has an immutable candidate
eligible for review or integration.
