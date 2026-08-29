# Audio1 exact repaired-candidate review: ACCEPT `1eed5b1`

- Reviewer: Codex Audio1 exact reviewer (different worker)
- Time: 2026-08-27T15:44:50-06:00
- **Decision: ACCEPT**
- Exact candidate: `1eed5b1b93616e5527d238e0d8fc1a14b149686d`
- Exact tree: `a2ce4da945dcd467bb088456d3be2a668798daf4`
- Parent: `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- Original Audio1 base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Findings in this exact candidate: **P1 0 / P2 0 / P3 0**

## Exact identity and repair closure

The reviewer independently proved `1eed5b1` is a direct descendant of the
run-scoped reset repair `bd3a94e`, detached at tree `a2ce4da...`, and clean.
The follow-up diff is exactly one insertion/one deletion in
`docs/wiki/development/testing-harness.md`: the canonical focused selector now
includes both `wireplumber-runtime` and `wireplumber-reset-lifecycle`. No
runtime/product source changed. `git diff --check` passes from both the original
Audio1 base and the immediate parent.

That one-line exact repair closes the selector-doc P2 posted against `bd3a94e`:
the documented command now discovers and executes all seven focused tests.
The cumulative runtime tree closes all six earlier P2s:

1. Every public client result class uses one queued, deduplicated, receiver-
   owned completion path; request IDs return first, completion is exactly once,
   explicit stop cancels undelivered local/accepted results and emits one queued
   `Uncertain/client-stopped` only for a dispatched mutation, while destruction
   safely drops delivery.
2. Backend values carry a fresh run generation; stop invalidates it before
   join, adapter/coordinator reject stopped and superseded values, and restart
   advances epoch before publication.
3. Client and coordinator reject old epochs, regressing revisions, and equal-
   revision content contradictions; authority loss makes dispatched work
   uncertain with no replay.
4. The untrusted backend outcome is validated atomically. Unknown status,
   malformed reason/diagnostic, or invalid lineage becomes the complete
   protocol-valid `Failed/backend-malformed` result.
5. Component loads and operation syncs are tracked/cancellable and their GLib
   callbacks drain before join. The ASan+UBSan runtime test completed 250 rapid
   start/stop cycles, enforced FD delta <=5, zero post-stop publication, and
   fresh-generation restart fencing.
6. PipeWire disconnect reset is an explicitly retained/destroyed GLib idle
   source carrying a worker-run token. Cleanup cancels it synchronously and
   dispatch rejects stale/stopped runs. The deterministic test pauses after
   source attach, queues higher-priority stop, and repeats two complete
   loss/stop/restart/second-loss cycles, proving exact epoch advances and second-
   loss publication without a stale latch/source.

The worker keeps every WirePlumber/GObject/source handle on its private GLib
thread. Only bounded copied values cross to Qt. The protocol/client/service
dependency direction remains one-way; WirePlumber and Threads are private
service dependencies, and no platform handle leaks through installed headers.

## Commands and direct evidence

All configurations were reviewer-owned, fresh, serialized, and built with
strict warnings. Host resource pressure required `-j1`; no gate was skipped.

- Debug configure/build (`BUILD_TESTING=ON`, Shell/KWin off): exit 0,
  **607/607**. The runtime-identical parent was built before the docs-only
  follow-up arrived; at exact `1eed5b1`, `cmake --build ... -j1` returned exit 0
  and `ninja: no work to do`.
- Debug `ctest -N` canonical selector: exactly **7**; full registry: exactly
  **90**. Focused: **7/7 passed**. Full: **90/90 passed**.
- Release fresh configure/build at exact `1eed5b1`: exit 0, **607/607**.
  Release discovery: **7** focused / **90** full; execution: **7/7** and
  **90/90 passed**.
- Debug and Release lifecycle selector
  `audio-(activation|wireplumber-(runtime|reset-lifecycle))` with
  `--repeat until-fail:10`: each configuration passed all three tests for all
  ten repetitions (**30 executions per configuration, 60 total**).
- Fresh sanitizer build with
  `-fsanitize=address,undefined -fno-omit-frame-pointer`: exit 0, **59/59**
  focused build steps. Exact discovery: **7**. With ASan leak detection and
  ASan/UBSan halt-on-error: **7/7 passed**, including the 250-cycle FD/callback
  barrier and deterministic reset-source lifecycle test.
- `./tools/validate-docs`: exit 0, **43 Markdown documents plus mkdocs.yml**.
- `./tools/check-source-shape --largest 20`: exit 0, **748 files**, zero
  allowlisted skips; repaired `wireplumber_worker.cpp` is **484 nonblank lines**.
- `uvx --offline --from mkdocs==1.6.1 mkdocs build --strict`: exit 0.
- `git diff --check dc29c889...HEAD` and immediate-parent whitespace check:
  exit 0.
- Fresh production Release (`BUILD_TESTING=OFF`, production Shell on, KWin
  off): exit 0, **403/403**.
- `cmake --build ... --target all_qmllint`: exit 0, **2/2** targets. The output
  retains existing nonfatal Shell QML warnings outside Audio1; no Audio1 QML is
  introduced by this candidate.
- `cmake --install`: exit 0, **157 staged files**. Verified exact installed
  Audio1 executable, public protocol/client/service libraries and headers,
  `org.qindaqt.Audio1.service`, `org.qindaqt.Audio1.xml`, and hardened systemd
  user unit. The staged descriptor points to the exact staged executable.

## Installed private-D-Bus lifecycle and cleanup

The reviewer invoked the installed D-Bus descriptor only on disposable private
`dbus-daemon` instances with reviewer-owned `XDG_RUNTIME_DIR` and unreachable
private PipeWire roots. Each activation checked `StartServiceByName`, exact
unique owner, `/proc/<pid>/exe` identity, and a real `GetSnapshot` response.
Killing the constructing daemon had to make that exact PID exit; a replacement
daemon then had to activate a distinct exact PID, distinct textual owner, and
distinct nonzero epoch before the second loss.

- Repeated complete installed lifecycle: **10/10 passed**.
- Exact staged service activations/exits: **20/20**.
- Replacement owner/PID/epoch distinct: **10/10**.
- Exact reviewed PIDs absent after the gate: **20/20**.
- Final exact staged service scan: **0 live**.
- Reviewer Audio fixture-root scan: **0 residual roots**.

The focused activation and private runtime tests likewise use private buses,
private PipeWire/WirePlumber processes, null devices, and exact child guards.
No host session bus, host audio graph/device, desktop, input, compositor,
KGlobalAccel, cursor, or lock action was contacted.

## Bounded caveats

This accepts the typed Audio1 process/protocol/client/service boundary and its
isolated runtime/activation/package evidence. It does not qualify physical USB,
HDMI, Bluetooth, jack, multichannel, microphone/speaker behavior, hotplug,
suspend/resume, realtime latency, hardware gain mappings, CPU/PSS budgets, or
future Audio Settings/shell UI. Those remain documented hardware and integrated-
session gates and were intentionally unavailable/unsafe for this review.

Final reviewer state is detached at exact `1eed5b1b93616e5527d238e0d8fc1a14b149686d`,
tree `a2ce4da945dcd467bb088456d3be2a668798daf4`, tracked-clean, with zero exact
review fixture processes or temp roots.

## Requested manager action

Integrate the exact accepted descendant
`1eed5b1b93616e5527d238e0d8fc1a14b149686d` (not `bd3a94e`, `e6423be`, prose,
or an uncommitted tree), then rerun the affected Audio1 selector and package
preflight on the integrated tree.
