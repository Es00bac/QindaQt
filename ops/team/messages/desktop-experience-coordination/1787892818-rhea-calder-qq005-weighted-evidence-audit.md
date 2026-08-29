# QQ-005 weighted integrated-evidence recommendation

- **Timestamp:** 2026-08-28T04:53:38Z
- **From:** Rhea Calder, Display D0 compositor-output lead — OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Scope:** bounded read-only KPI evidence audit for QQ-005 Platform services
- **Evidence identity:** public main `2c52c985f846b083c2aebb7a08f04aa8318a2912`
- **Product/Git/build/runtime mutation:** none; only this board recommendation and Rhea's own worker record changed

## Recommendation

Use nine stable user-outcome steps with weights totaling exactly 100. The weights
describe the share of the QQ-005 outcome, not worker effort, message volume, or
candidate size. A step's stage is supported only by accepted evidence already
integrated on public main. Architecture plans and preserved worker candidates can
explain the next transition, but cannot advance the integrated stage.

Interpret the evidence stages as follows:

- `ABSENT`: no integrated implementation of the named QQ-005 outcome.
- `MODELLED`: an integrated bounded domain model/contract and deterministic model
  tests exist, but no live provider/client path is connected.
- `WIRED`: the integrated provider, client, and required authorities/consumers are
  composed, but no accepted end-to-end executable proof exists.
- `EXECUTABLE`: an accepted integrated stack operates in its declared isolated
  runtime/package boundary, while named user-surface, hardware, or broader
  qualification remains.
- `QUALIFIED`: the step's complete declared user outcome, failure behavior,
  accessibility, persistence where applicable, and required acceptance matrix are
  independently accepted and integrated.

This rubric deliberately does not turn the ordinal stages into invented fractional
credit. The manager may report the distribution of weighted scope by stage; a
weight becomes completed scope only at `QUALIFIED` unless a separate board policy
defines a stage-to-percent conversion.

| Weight | Stable user outcome | Integrated stage at `2c52c985` | Exact evidence and caveat |
|---:|---|---|---|
| 14 | **Audio device/default/stream control.** Users can ultimately inspect real inputs, outputs, and application streams and change default device, level, mute, and stream target with truthful reconnect/error behavior. | `EXECUTABLE` | Integrated functional stopping point `fac2756a65572f37296c0fb6bd38b74aa68574d3` is an ancestor of `2c52c985`. Public main contains `src/services/audio_{protocol,client,service}/**` and all focused tests under `tests/services/audio_{protocol,client,service}/**`. `docs/HANDOFF.md:5-48` records independent acceptance, Debug/Release 749/749, Audio 7/7, broad 108/108, sanitizer, package, activation, and installed lifecycle evidence. `docs/wiki/development/testing-harness.md:630-683`, `docs/wiki/architecture/audio-service.md:13-167`, `docs/wiki/reference/audio1-v1.md`, and ADR-0014 record the executable private WirePlumber/PipeWire boundary. It is not `QUALIFIED`: physical microphone/speaker, USB/HDMI/Bluetooth/jack/multichannel, suspend/hotplug/latency/resource budgets, and Audio Settings/shell UI remain explicitly unqualified (`testing-harness.md:679-683`; `audio-service.md:132-167`). |
| 18 | **Display inventory and reversible output transactions.** Users can identify outputs, arrange/enable/rotate/scale/mirror/select mode and primary, preview with a visible deadline, and reliably confirm or revert across hotplug/restart. | `ABSENT` | Public main has no `src/services/display_{protocol,client,service}` or transaction module. The qualified QQ-002 compositor is a prerequisite, not this QQ-005 transaction outcome. D0 is preserved only as 50 uncommitted source/static paths at base `94e84077` with no compiler/runtime/review (`display-platform-architecture/1787891566-rhea-calder-d0-source-static-reentry-handoff.md:6-12,59-81,106-110`). D1 remains failed-candidate HEAD `0e38fa726af69e34be3cacdd6b71d40350ac8092` plus 15 uncommitted repair paths (`1787891180-kellan-ward-display-d1-source-ready-compiler-wait.md:7-10,34-58`); Mina's PASS reserves compile/package/runtime and exact-commit rereview (`1787891900-mina-shah-repair-rereview-pass.md:106-118`), and `1787892261-kellan-ward-d1-parallel-compile-qualification-claim.md` is only a lane claim. None is integrated progress. If accepted later, D1's pure identity/topology/transaction slice and D0's compositor inventory seam would first justify `MODELLED`, not `WIRED`. |
| 13 | **Power status/actions and coherent brightness.** Users can understand batteries/UPS/charging/profile/inhibitors, safely request session power actions, and adjust each display or keyboard backlight without duplicate or fabricated controls. | `ABSENT` | Public main has no `power_{protocol,client,service}` or `brightness_model`. This is one coherent user outcome while retaining the planned implementation boundary: Power1 owns UPower/optional power-profiles-daemon/logind state and actions; brightness composes public Power/Display clients and adds no daemon. Display acceptance remains a prerequisite for output brightness. No worker proposal counts as integration. |
| 11 | **Network connectivity.** Users can see authoritative connectivity and active profiles, scan, connect/disconnect known networks, and control radios without QindaQt owning routing, persistence, or secrets. | `ABSENT` | Public main has no `network_{protocol,client,service}`. NetworkManager/KF6 NetworkManagerQt reuse, scan leases, owner replacement, and the separate secret-agent boundary remain architecture only; there is no integrated model, provider, client, package, or private-bus evidence. |
| 9 | **Bluetooth devices.** Users can inspect adapter/device state, discover, and connect/disconnect already-paired devices; later pairing prompts fail closed and never duplicate BlueZ trust/key authority. | `ABSENT` | Public main has no `bluetooth_{protocol,client,service}`. BlueZ/BluezQt reuse and the separate Agent1/pairing boundary remain planned only. Bluetooth audio correlation belongs after both Audio and Bluetooth bases and must not be inferred from Audio's null-device proof. |
| 9 | **Private bounded clipboard history.** Users retain normal Wayland clipboard behavior and can optionally search/reselect/paste/delete/clear volatile history that purges and denies access on lock or authority loss. | `ABSENT` | Public main has no `clipboard_{protocol,client,host}`. The existing authenticated lock-state foundation is adjacent QQ-004 work, not Clipboard1 progress. `ext-data-control`, authenticated private access, strict byte/MIME bounds, FD lifecycle, lock purge, and mixed-toolkit nested evidence all remain unimplemented for QQ-005. |
| 8 | **Display color, ICC, HDR, and WCG settings.** Users can inspect/import/assign profiles and see whether the compositor can apply the requested output color state. | `ABSENT` | Public main has no `color_{protocol,client,service}`. Design-token color math is presentation infrastructure, not colord catalog/assignment or compositor application. Color depends on an accepted Display client and still needs bounded ICC-by-FD validation, split catalog/apply failure truth, and nested/hardware color evidence. |
| 6 | **Font discovery and confirmed first-party application.** Users can search/preview families and choose UI/monospace, size, antialiasing, hinting, and subpixel settings that QindaQt apps apply before QML construction. | `ABSENT` | Public main has no `font_{protocol,client,service}` or the planned app-bootstrap integration. Settings1 and QST are prerequisites, not a fontconfig catalog, typed Font1 client, isolated derived fragment, or application proof. |
| 12 | **Portal interoperability and desktop policy export.** Sandboxed apps receive qualified chooser/open-URI/notification/inhibit/screencast/remote-desktop behavior and authoritative QindaQt appearance values without forking the portal frontend or silently replacing consent policy. | `ABSENT` | Public main has no `portal_settings_backend`. Existing Settings1/QST, notifications, KWin, and the installed KDE portal ecosystem are prerequisites only. Mixed-backend selection, QindaQt Settings export, parent/cancel/consent/accessibility behavior, and nested/Flatpak portal requests have no integrated QQ-005 evidence. |

**Weight check:** `14 + 18 + 13 + 11 + 9 + 9 + 8 + 6 + 12 = 100`.
Current integrated stage distribution is therefore `EXECUTABLE: 14` and
`ABSENT: 86`; `MODELLED`, `WIRED`, and `QUALIFIED` each hold zero weighted
scope under this decomposition. This is a distribution, not a new completion
percentage.

## Integrated-evidence audit

The current board state remains honest: `ops/team/features.json:66-78` calls
QQ-005 `MODELLED`, records only the integrated Audio stopping point, and says
the milestone is not executable from one provider. I recommend retaining that
milestone-level state until accepted cross-domain breadth changes the stopping
point.

Read-only checks against exact public main established:

- `git merge-base --is-ancestor fac2756a65572f37296c0fb6bd38b74aa68574d3 2c52c985f846b083c2aebb7a08f04aa8318a2912` exited 0.
- `git ls-tree -r --name-only 2c52c985... src/services` and exact tree-object probes found all three planned Audio directories present and all 23 expected non-Audio QQ-005 directories absent: Display 3, Power 3, brightness 1, Network 3, Bluetooth 3, Clipboard 3, Color 3, Font 3, and portal settings 1.
- `docs/wiki/development/implementation-roadmap.md:93-100` marks Platform services in progress and keeps failure/accessibility/persistence/tests/display evidence/docs in its completion contract. `docs/TASK_LIST.md:3-5,21-23,37-44` says partial work does not count, records Audio complete, and leaves other providers later. `docs/HANDOFF.md:5-48,59-62` records the accepted Audio evidence and explicitly treats Display as source-only worker work. `docs/wiki/architecture/module-boundaries.md:39-42,54-55,73-84,119-121` exposes only the accepted Audio service boundary among these providers and states that planned directories do not exist until implemented.
- Samira Cole's architecture handoff `platform-services/1787853847-samira-cole-plan-handoff.md:162-501` supplies the domain separation and user-outcome rationale used here. It is design authority, not integrated-product evidence.

## Transition and manager action

Adopt this nine-step/100-weight ledger as the stable QQ-005 measurement
decomposition. Keep exact commit/source/test references on each future stage
transition. An assignment, candidate, compile claim, reviewer prose, or clean
worker tree must not move a step. Movement requires an accepted exact commit,
manager integration, verification on the integrated tree, and an evidence-backed
board update under the live-board gate.

For the preserved Display work specifically, eventual integration of the pure D1
model and D0 compositor observation seam would establish a meaningful
`MODELLED` stopping point. Display becomes `WIRED` only when a provider/client can
apply against the supported backend with owner/lifetime truth; it becomes
`EXECUTABLE` only after the private preview/confirm/revert and crash/timeout
workflow passes; it becomes `QUALIFIED` only after the required accessibility,
failure, persistence, nested display matrix, and declared hardware boundary are
accepted.
