# Ada Ruiz — decision-complete candidate handoff

- **Timestamp:** 2026-08-27T03:43:39Z
- **Provider/model evidence:** OpenAI, exact `gpt-5.6-sol`, reasoning high
- **Base:** `496e5135ee4f40359f8b871eec130f0b8b02a241`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Candidate:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- **Tree:** clean after the single candidate commit
- **Size:** 90 changed paths; 5,442 insertions; 233 deletions

## Outcome delivered

The candidate lands immutable schema v1 plus active v2 with the dedicated
Boolean `services.doNotDisturb=false`, validated v1-to-v2 migration, recursive
JSON-native bounded Settings1 v1, an independently D-Bus-activatable service,
copy-on-write atomic persistence/publication, and an explicit nonwrapping
process-lineage revision counter with exercised exhaustion.

It also lands the exact-owner async client (activation, subscribe-before-read,
owner/epoch/revision fencing, timeout, replacement, local bus loss, and
uncertain-write resync without replay), DND controller, fail-quiet/last-confirmed
shell bridge, lock-privacy precedence, read-only presentation projection, the
Settings1-backed center control and fixed route, and the ordinary accessible
`qindaqt-settings --page notifications` application/desktop entry. It does not
add a settings applet, layer-shell settings UI, shortcut, or supervisor child.

Major path groups are `data/settings`, `src/settings`,
`src/services/settings_{protocol,service,client}`, `src/apps/settings_center`,
the bounded shell/presentation integration paths, their focused tests, and
ADR-0012 plus affected wiki/task/handoff pages.

I inspected Mira's paused partial worktree read-only and deliberately adopted
compatible schema/migration/protocol/service structure into my clean tree via
patches. I independently implemented the complete client/UI/shell outcome,
corrected the codec to recursively support nested Object values with per-value
and aggregate snapshot/transaction bounds, and independently built/tested this
candidate. I make no claim over Mira's partial implementation or test runs.

## Verification evidence

- Debug full registry: `ctest --test-dir build/ada-debug -R '^qindaqt\\.'
  --output-on-failure -j2` — exit 0, **66/66 passed**.
- Release full registry: `ctest --test-dir build/ada-release -R '^qindaqt\\.'
  --output-on-failure -j2` — exit 0, **66/66 passed**.
- Production Release: KWin plugin off, production shell on; full build and
  `qindaqt-shell`, `qindaqt-settings`, `qindaqt-settings-service` — exit 0.
- QML lint: `all_qmllint` — exit 0. It reports only existing warnings in
  unrelated preview components; no new settings/notification-center warning.
- Docs: `tools/validate-docs` — exit 0, **42 documents**; isolated MkDocs 1.6.1
  `mkdocs build --strict` — exit 0.
- Shape/diff: `tools/check-source-shape --largest 20` — exit 0, **758 files,
  zero skips and no decomposition warning**; `git diff --check` and cached diff
  check — exit 0.
- Staged install: production Release installed under the isolated
  `build/ada-stage-prefix`; service executable, shell, app, desktop entry,
  schemas, public headers, and activation descriptor were present.
- Installed activation: private `dbus-run-session` discovered the staged
  descriptor, activated `org.qindaqt.Settings1`, and returned status Applied,
  wire schema **1**, settings schema **2**, revision **0**, DND **false**, and
  source `system-defaults` — exit 0.

## Bounded caveats and requested action

Per the accepted test boundary, no live compositor, real user session bus,
assistive-technology bridge, lock screen, KGlobalAccel, uinput, pointer, or
keyboard automation was used. Offscreen structural/accessibility and isolated
private-bus lifecycle coverage replaces those unsafe/host-dependent probes.

Please assign a different-provider reviewer to review the exact candidate
`00b3d49ac3d7ba94edcf10272fa5e61185d63b56`. Review should especially inspect
recursive/aggregate bounds, persistence ordering and exhaustion, exact-owner
client fencing/uncertain writes, fail-quiet plus privacy precedence, QML
state/focus semantics, activation/install paths, and docs/task truthfulness.
