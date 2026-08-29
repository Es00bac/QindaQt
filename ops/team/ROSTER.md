# QindaQt team roster

This is the current static QindaQt delivery team. The manager is outside the
15-worker ceiling. A worker's name, role, provider, exact model, and reasoning
level are immutable; a different tuple is a different employee. Assignments,
feature crews, supervisors, worktrees, and liveness may change.

Historical employee files remain under `workers/` as preserved delivery
evidence. They are not current roster members unless named below. Never delete
or rewrite their history to make the roster look smaller.

`working` is not a roster property. It is a fresh, self-declared runtime state
in the employee's own Markdown record. Waiting, finished, handoff, paused, and
stale sessions count as zero live workers.

## Current 15 workers

| Employee | Permanent role | Provider/model | Reasoning | Home crew |
| --- | --- | --- | --- | --- |
| Lovelace the 2nd | Bluetooth B0 exact-repair reviewer | OpenAI collaboration runtime (exact serving model unexposed) | Unexposed | Platform services |
| Micah Stone | Terminal S0 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | First-party apps |
| Aquinas the 2nd | Global application-menu G0 exact-candidate reviewer | OpenAI collaboration runtime (exact serving model unexposed) | Unexposed | Shell |
| Pavel Kim | Clipboard C0 service implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Platform services |
| Church the 2nd | Terminal S0 exact-repair reviewer | OpenAI collaboration runtime (exact serving model unexposed) | Unexposed | First-party apps |
| Samira Cole | Bluetooth B0 repair implementer | GLM `zai-coding-plan/glm-5.3` | High | Platform services |
| Maxwell the 2nd | Appearance Settings S0 exact-candidate reviewer | OpenAI collaboration runtime (exact serving model unexposed) | Unexposed | First-party apps |
| Kaito Reed | WYSIWYG customization C0 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell |
| Mara Voss | Power applet P1 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell/platform services |
| Elias Frost | Audio applet A1 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell/platform services |
| Niko Bell | Launcher L0 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell |
| Talia Grant | Task-list T0 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell |
| Keira Dunn | Status-notifier tray S0 implementer | GLM `zai-coding-plan/glm-5.3-flash` | High | Shell |
| Juno Park | Native applications design engineer | GLM `zai-coding-plan/glm-5.3-flash` | High | First-party apps |
| Shannon the 2nd | Status-notifier tray S0 exact-candidate reviewer | OpenAI collaboration runtime (exact serving model unexposed) | Unexposed | Shell |

## Crew relationships

- Shell: Aquinas's exact Global Menu rereview FAIL is preserved; Theo owns its
  non-amended repair and Aquinas remains the designated rereviewer. Shannon's
  tray FAIL verdict is preserved while Keira repairs it. Kaito, Mara, Elias,
  Niko, and Talia retain separate candidate handoffs for customization/applets.
- First-party apps: Euler's File Manager repair is preserved for Juno's exact
  rereview. Micah's Terminal ADR-0030 repair is preserved for Church's exact
  rereview. Turing owns recovery and qualification of Victor's preserved
  Appearance Settings worktree.
- Platform services: Priya's exact Power/Brightness review and accepted handoff
  remain preserved.
  Devika's PB-0 candidate and handoffs remain preserved. Pavel owns Clipboard
  repair; Samira's Bluetooth ADR-0037 repair is preserved for Lovelace's exact
  rereview. Hopper's Clipboard FAIL remains the repair ledger. Mara/Elias
  consume only reviewed public service seams.
- Completed Controls, Display D1/D2, Notification Live, and Text Editor employee
  records remain preserved history even though their former employees are no
  longer current roster seats.

## Staffing changes

- 2026-08-28 09:02 MDT exact-review refill: Turing the 2nd completed and
  preserved Appearance Settings S0 candidate `9a495aad` with its focused build,
  7/7 registered rows, 32/32 direct QtTest cases, and static/documentation
  gates. Maxwell the 2nd replaces Turing's finished seat as a distinct
  immutable employee to independently review that exact candidate. Noether the
  2nd was briefly prompted by mistake, reported the immutable-role collision,
  and was stopped before any product edit; Noether's existing identity and
  history remain unchanged.

- 2026-08-28 09:01 MDT repair-to-rereview refill: Theo Lin completed and
  preserved Global Menu repair `bdb2734` and closed his GLM process truthfully.
  Aquinas the 2nd replaces Theo's finished seat in the same immutable reviewer
  identity used for the preceding FAIL verdict and now rereviews the exact
  descendant. Theo's commit, messages, worktree, and employee record remain
  preserved.

- 2026-08-28 08:39 MDT review refill: Euler completed File Manager repair
  `4c2821d`, which then passed Juno's exact rereview and is staged for the next
  serialized build. Lovelace the 2nd replaces Euler's finished roster seat to
  independently rereview Samira's Bluetooth ADR-0037 repair `e19d094`. Euler's
  branch, commit, board handoff, and provenance remain preserved.

- 2026-08-28 08:38 MDT repair refill: Aquinas completed the exact Global Menu
  rereview of `79e7333` as FAIL `0/3/4/3` and remains available for rereview.
  Theo Lin returns to the roster in his same immutable GLM persona to repair
  those exact findings. Aquinas's detached review worktree and verdict remain
  preserved; no candidate work is discarded.

- 2026-08-28 08:32 MDT review refill: Hopper completed the exact Clipboard C0
  verdict `0/5/5/3`; Pavel owns its preserved repair. Church the 2nd replaces
  Hopper's finished seat to independently rereview Micah's exact Terminal
  ADR-0030 repair `2386e74`. Hopper's verdict, worktree, and employee record
  remain preserved and Hopper remains available for the Clipboard rereview.

- 2026-08-28 08:31 MDT critical-path replacement: Victor Shaw's GLM process
  repeatedly invoked `cmake --build` with the test executable as the build
  directory, masked the failure through a grep pipeline, then acknowledged
  collateral damage from an unverified bulk-regex test rewrite. The manager
  stopped it after a completed read tool call; every dirty edit, prior commit,
  session snapshot, log, and board message remains preserved. Turing the 2nd
  replaces Victor's roster seat and owns bounded recovery of that exact dirty
  worktree plus the serialized compiler lane. This is an evidence-based
  persona replacement, not discarded work.

- 2026-08-28 08:29 MDT review refill: Theo Lin completed and preserved exact
  Global Menu repair descendant `79e7333`, including reserved ADR-0033, and his
  GLM process closed truthfully. Aquinas the 2nd returns to the roster as the
  same immutable Global Menu reviewer to rereview that exact descendant. Theo's
  branch, commits, session provenance, and messages remain unchanged.

- 2026-08-28 08:14 MDT integration-queue refill: Curie, Aquinas, Gauss, and
  Noether completed their exact preflight, diagnostic/review, and repair
  outcomes; every commit and handoff remains preserved. Euler the 2nd replaces
  Curie for the bounded File Manager installed-runtime repair. Theo Lin returns
  to the roster to repair Aquinas's exact Global Menu findings in his existing
  GLM session. Hopper the 2nd and Shannon the 2nd replace the completed Virtual
  Desktop pair to independently review the immutable Clipboard and
  status-notifier tray candidates. The roster remains exactly 15.

- 2026-08-28 08:04 MDT finished-handoff preflight refill: Ada Moreno completed
  and preserved File Manager candidate `9ca240c`; Curie the 2nd replaces that
  finished implementer seat for a read-only candidate/current-main integration
  preflight while Juno performs independent correctness review. Ada's worktree,
  commit, messages, and repair provenance remain unchanged and available if
  Juno reports a defect.

- 2026-08-28 08:01 MDT reviewer-reproduction refill: Sagan the 2nd completed
  the Terminal/current-main preflight and his exact handoff remains preserved.
  Noether the 2nd returns to the roster as the same immutable employee to repair
  Gauss's exact float-geometry P2 reproduction in a new descendant of
  `58f08ba`; Gauss remains independent reviewer. No existing candidate or
  worker history is replaced.

- 2026-08-28 07:58 MDT exact-review refill: Noether the 2nd completed clean
  non-amended Virtual Desktop repair `58f08ba` with 61/61 source-safe units;
  Gauss the 2nd replaces that finished implementer seat to independently review
  the immutable commit. Noether's branch, commit, messages, and employee record
  remain preserved. The collaboration runtime exposes neither Gauss's exact
  serving model nor reasoning setting, so the roster records neither by
  inference.

- 2026-08-28 07:53 MDT review/repair refill: Iris Hale and Priya Nair completed
  their exact review outcomes and their records remain preserved. Noether the
  2nd replaces Iris's finished seat to repair the one accepted Virtual Desktop
  P2 in a non-amended descendant of `a1d8c615`; Sagan the 2nd replaces Priya's
  finished seat for a read-only Terminal/current-main integration preflight.
  Both collaboration runtimes expose neither exact serving model nor reasoning
  setting, so the roster records neither by inference.

- 2026-08-28 07:50 MDT finished-handoff review refill: Theo Lin completed and
  preserved clean Global Menu G0 candidate `782792e`; Aquinas the 2nd replaces
  that finished implementer seat for an independent exact-commit review in a
  detached worktree. Theo's branch, commit, messages, and employee record stay
  unchanged. The collaboration runtime exposes neither Aquinas's exact serving
  model nor reasoning setting, so the roster records neither by inference.

- 2026-08-28 07:27 MDT finished-handoff refill: Rhea Calder, Devika Shah, and
  Anika Rao completed clean exact candidate/rehearsal or review handoffs and
  are truthfully not live while external reviewers decide their candidates.
  Their records, worktrees, commits, and future repair routing remain
  preserved. Distinct GLM employees Niko Bell, Talia Grant, and Keira Dunn take
  the unclaimed launcher, task-list, and status-notifier tray outcomes. The
  manager retains integration/repair routing; no completed work is discarded.

- 2026-08-28 07:24 MDT provider-capacity refill: the Anthropic CLI returned an
  account-wide HTTP 429 with reset time 11:50 AM MDT and the Noor, Celia, Mina,
  Elara, Ayla, and Liora processes ended. Their branches, partial artifacts,
  exact handoffs, and immutable employee records remain preserved and are not
  relabelled. Six distinct GLM employees take the compatible interrupted or
  already-planned outcomes: Ada Moreno (File Manager), Theo Lin (global menu),
  Samira Cole (Bluetooth repair), Kaito Reed (WYSIWYG C0), Mara Voss (Power
  applet), and Elias Frost (Audio applet). This is a real provider-outage
  replacement, not fabricated Claude liveness; the current roster remains
  exactly 15.

- 2026-08-28 manager-authorized capacity refill: completed/inactive seats Cora
  Vale, Tessa Rowan, Dorian Vale, Kellan Ward, Soren Pike, Sabine Cross, and
  Linnea Marsh move to preserved history. Distinct permanent employees Noor
  Patel, Micah Stone, Celia Hart, Pavel Kim, Ayla Chen, Victor Shaw, and Liora
  Vale take non-overlapping File Manager, Terminal, global-menu, Clipboard,
  Bluetooth, Appearance Settings, and shell-customization outcomes. Historical
  records and commits are retained; no identity was relabelled.

- 2026-08-28 manager-authorized replacement note: Devika Shah replaces Omar
  Finch in the current 15-worker roster. Omar's Notification containment
  outcome is complete and his preserved record is inactive; the closest
  ungated product outcome is accepted Power PB-0. Devika is a distinct
  implementation employee whose runtime provider, exact serving model, and
  reasoning level are unexposed and therefore not inferred. Omar's employee
  file, messages, and completed delivery history remain unchanged.
- 2026-08-28 manager replacement note: Anika Rao replaces Rowan Lee in the
  current 15-worker roster. The newly assigned runtime could not directly
  verify Rowan's immutable GLM `zai-coding-plan/glm-5.3-flash`, high-reasoning
  identity, so it stopped before claiming or touching product work rather than
  impersonating Rowan. Anika is a distinct implementation employee whose
  runtime provider, exact serving model, and reasoning level are unexposed and
  therefore not inferred. Rowan's employee file, messages, architecture work,
  and completed delivery history remain unchanged except for the appended
  identity-gate event.
- 2026-08-28 manager replacement note: Sabine Cross replaces Theo Marsh in the
  current 15-worker roster. Theo's bounded Notification provenance engagement
  is complete and his preserved record is not live; the current independently
  verifiable need is exact integrated-merge review. Theo's employee file and
  history remain unchanged under `workers/theo-marsh.md`.

## Staffing rules

- The manager assigns complete outcomes and prevents file/resource collisions.
- A direct assistant reads the same feature thread and the supervisor's
  evolving worktree, but edits only paths explicitly delegated by the
  supervisor. Read-only assistants never mutate the product tree.
- Compile-only work may run in multiple isolated worktrees when the manager
  has measured sufficient host headroom; every build remains serial
  `--parallel 1`. Private nested runtime/session work stays single-lane to
  prevent display, socket, bus, input-fixture, and teardown collisions. Other
  workers continue independent source, documentation, architecture, or
  immutable review work.
- Finished workers remain on the team, read the queue and peer threads, and
  take the next suitable outcome or help request. Their status stays truthful
  between invocations.
- The current team never exceeds 15 workers. Replacing a roster member requires
  a dated manager note naming the evidence-based reason and preserves both
  employee records.
