# Independent exact-candidate review — Clipboard C1 service

- **Reviewer:** Evelyn Berezin (`evelyn-berezin`), independent platform-service reviewer
- **Provider/model:** Moonshot Kimi `kimi-code/k3-256k`, reasoning high
- **Candidate SHA:** `405577cc964dd1282a9210af282650a132391056`
- **Candidate tree SHA:** `09cedd7da0d052b980deeb4e1a1f6ef19c71b6e7` (matches handoff)
- **Parent SHA:** `ce9228d9694622d503d92a38d01986f8f124f188` (sole parent; equals declared base)
- **Base SHA:** `ce9228d9694622d503d92a38d01986f8f124f188`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-service-c1-k3-review` (detached at candidate)
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-clip-k3`
- **Hygiene:** `git rev-parse HEAD` = candidate; `git status --porcelain` empty before and after all work. No product path was edited. All scratch reproductions live under `<ROOT>/scratch/`.

## Findings ledger

### P0 — none

### P1 — 1

#### P1-1: First start captures selections without explicit user opt-in (schema default `true` is treated as confirmed consent)

The candidate ships the resident host and its D-Bus activation artifacts, and
the host's only consent check is in `src/services/clipboard_service/app/main.cpp:48-58`:

```cpp
service.host()->setHistoryOptIn(value.metaType().id() == QMetaType::Bool
                                && value.toBool());
```

`value` comes from `SettingsSnapshot::values`, which the Settings1 repository
populates through layered resolution (`settings_repository.cpp:41-46`,
`LayeredSettings::value` → `resolve`, `layered_settings.cpp:35-40,154-164`).
The `SystemDefaults` layer is seeded from the schema, and both shipped schemas
declare `"default": true` for `services.clipboardHistory`
(`data/settings/schema-v1.json:228`, `data/settings/schema-v2.json:249`; the
production settings service loads v2 first, `settings_service/src/main.cpp:50`).
So on a fresh install with no user overrides, the first confirmed Settings1
snapshot delivers `services.clipboardHistory = QVariant(bool, true)` sourced
from `SystemDefaults`, the predicate above evaluates to `true`, and — once the
authenticated lock monitor reports Unlocked and the data-control device is
available — `setCaptureEnabled(true)` starts recording every selection. The
user never opted in.

The candidate's own architecture page claims the opposite:
`docs/wiki/architecture/clipboard-service.md:222-225` says "the host
deliberately does not infer consent from that declaration and remains disabled
until Settings1 returns a confirmed value." Functionally it *does* infer
consent from the declaration: Settings1 resolution conflates the schema
default into a confirmed Boolean `true`, and the host ignores the
`sourceLayers` map that would let it tell the difference
(`settings_client.h:31`; wire layer names in `settings_types.cpp:15-20`;
service-side encoding in `settings_object.cpp:43-50,223`). This violates the
accepted opt-in-and-off-by-default architecture (ADR-0031; manager note for
this lane).

**Reproduction** (scratch binary `<ROOT>/scratch/optin_repro`, links
`qindaqt_settings` from the Debug tree, loads the shipped schema files exactly
as the production service does, no user overrides):

```
data/settings/schema-v2.json value= QVariant(bool, true) type= bool sourceLayer= 0 (0=SystemDefaults 2=UserOverrides)
data/settings/schema-v2.json host setHistoryOptIn(true) from this snapshot? true
```

Observed: fresh resolution yields Boolean `true` from `SystemDefaults`, and the
exact predicate from `main.cpp` returns true. Expected per ADR-0031 and the
architecture page: capture stays off until the user explicitly enables it.
(The v1 file fails to load in this probe only because the probe compiled with
`QINDAQT_SETTINGS_SCHEMA_VERSION=2`, matching production behavior; the v1 file
carries the same `true` default as migration source.)

**Precise repair (manager to authorize):**

1. *Schema-owner (primary, required for the shipped default):* change
   `"default": true` → `"default": false` for `services.clipboardHistory` in
   `data/settings/schema-v2.json:249` (active schema) and
   `data/settings/schema-v1.json:228` (migration source), with the schema
   documentation/fixture updates that accompany a default change.
2. *Clipboard-lane defense in depth (recommended, within the candidate's own
   ownership):* in `main.cpp`, accept `true` as consent only when
   `snapshot->sourceLayers.value("services.clipboardHistory")` equals
   `"user-overrides"`, so no future schema or profile default can become
   implicit consent again. The client snapshot already transports this map;
   no protocol change is needed.

Either repair alone closes the first-start capture; both together make the
host fail-closed against schema regressions.

### P2 — 1

#### P2-1: Remembered-request cache has no eviction — a caller is permanently `Busy` after 64 mutations

`src/services/clipboard_service/src/clipboard_service_object.cpp:72-82`:
once a caller's `requests` map reaches `kMaxRememberedRequestsPerCaller` (64),
every *new* request id is answered `Busy` / `request-cache-full`. Nothing ever
evicts a remembered result; entries leave only via `forgetCaller` when the
caller's unique name vanishes (`resident_clipboard_service.cpp:92-99`). A
long-lived shell client that performs 64 Select/Delete/Clear/Copy operations
over the resident host's lifetime can never mutate again without
disconnecting and reconnecting (new unique name). 64 history operations is a
low ceiling for a weeks-long shell session — every copy-from-history consumes
one permanently. `docs/wiki/reference/clipboard1-v1.md:53-54` discusses
eviction semantics ("Eviction of an old remembered result never permits a
client to assume replay safety") for an eviction mechanism the implementation
does not have.

**Reproduction** (scratch binary `<ROOT>/scratch/cache_repro`, drives the real
`ClipboardServiceObject` + `ClipboardHost` from the Debug static libraries
with the lane's `FakeWaylandAdapter`; one Clear per distinct request id,
lineage refreshed from the live snapshot each time):

```
first Busy at request id 65 reason "request-cache-full"
succeeded: 64 firstBusy: 65
fresh id 1000 after ceiling: 4 "request-cache-full"
```

Observed: ids 1–64 succeed; id 65 and any later fresh id return `Busy`
forever. Expected: either bounded LRU eviction as the reference implies, or
explicit documentation that the cap is a hard per-connection lifetime limit.
(The `replay of id 1: request-id-conflict` line in the same run is correct
behavior — my replay carried a refreshed revision, i.e. different arguments.)

### P3 — 3

#### P3-1: Caller bound off-by-one

`clipboard_service_object.cpp:72-73` rejects a new caller only when
`m_remembered.size() > kMaxRememberedCallers` (64) — the 65th distinct caller
is admitted, so the actual ceiling is 65 versus the documented "at most 64
callers" (`clipboard1-v1.md:51`). Bounded and harmless; precision only.

#### P3-2: Advertised MIME names and pending offers accumulate without a bound

`qt_wayland_clipboard_adapter.cpp:51-54` appends every `offer` event's MIME
name to an unbounded `QStringList` before classification, and
`introduceOffer` (lines 204-207) appends every `data_offer` to `m_offers`,
which drains only when a selection event names the offer. Both run regardless
of `m_captureEnabled`. Classification caps what is *read* (8 formats, 1 MiB —
verified), but not what is *accumulated* from the wire. Attack reachability is
real but throttled: offers arrive only from the SO_PEERCRED-authenticated
compositor (relaying client applications' MIME lists), and a hostile burst
overflows the Wayland wire buffer first — observed with a 200,001-MIME-type
offer from the lane's fake server: `Data too big for buffer (4096 + 32 >
4096)`, `error in client communication`, and the adapter failed closed with no
capture, no refusal, and no crash after 60 s. So the impact is slow-drip
memory growth in the resident host, not corruption. A count/byte cap on
advertised names per offer and on pending unselected offers would close it.

#### P3-3: No negative control for compositor disconnect / global removal

The `transportLost` fail-closed path
(`qt_wayland_clipboard_adapter.cpp:428-438`) and `globalRemoved`
(258-264) are not exercised by any test row; the fake-server suite proves
offer/selection/primary, sensitive no-read, and the 1 MiB ceiling only. Code
read shows the path withdraws availability, cancels in-flight transfers, and
disables notifiers correctly, so this is a coverage gap, not an observed
defect.

## Review questions (evidence summary)

1. **Privacy and bounds.** No payload bytes reach disk, logs, D-Bus signals,
   or error strings: snapshots carry only QCDL descriptors
   (`clipboard_host.cpp:24-36`); `captureRefused` is deliberately silent
   (`clipboard_host.cpp:86-91`); `main.cpp` logs static strings only; the
   boundary-poison gate rejects `QFile`/`QSaveFile`/`QStandardPaths`/
   `qDebug`/`qInfo` across all four production modules. Sensitive and
   one-time markers refuse the whole offer before any pipe is opened (proven
   by `refusesSensitiveWithoutReadingPayload`: zero `receive` requests).
   Unknown/non-canonical MIME types (incl. uppercase `SAVE_TARGETS`-style
   tokens after canonicalization) fail closed as non-storable. Reads are
   bounded at 1 MiB aggregate per item (proven by `rejectsOversizedTransfer`)
   and non-blocking on the Qt thread. Gap: P3-2 (pre-classification
   accumulation unbounded) and P1-1 (consent).
2. **Lock and opt-in gating.** Capture requires opt-in ∧ conclusively
   Unlocked ∧ device available (`clipboard_host.cpp:48-71`); both authority
   bits default false; Settings1 non-Ready state forces opt-in false
   (`main.cpp:59-64`); no Wayland peer → no lock monitor → never unlocked
   (fail closed). Disabling purges the model and raises the generation exactly
   once (proven by `disablingPurges`). Defeated only by P1-1's consent
   conflation.
3. **Wayland adapter correctness.** Pinned XML is byte-identical to the
   installed staging protocol
   (`sha256 293deba5…b7fb` both for
   `/usr/share/wayland-protocols/staging/ext-data-control/ext-data-control-v1.xml`
   and `src/services/clipboard_wayland_adapter/protocol/ext-data-control-v1.xml`);
   a checksum gate (`check_boundary.cmake`) pins it. Destruction order in
   `stop()` is offers → sources → device → manager → seat → registry →
   display; fds are closed on complete/cancel/refuse/destructor paths on both
   the receive and send sides; reads are non-blocking with a 16 KiB drain
   loop. Fake server proves offer/selection/primary flows and oversize
   refusal; disconnect is untested (P3-3).
4. **Lineage.** Exact-owner binding, token-fenced late-reply rejection,
   epoch/generation/revision validation, contradictory-lineage withdrawal,
   owner-loss withdrawal, and idempotent exact-duplicate results with
   conflict rejection are all implemented and test-proven
   (`tst_clipboard_client.cpp`, `tst_clipboard_private_bus.cpp`). No mutation
   is ever replayed; timeouts complete as Uncertain. Gap: P2-1 (cache
   ceiling), P3-1 (caller off-by-one).
5. **Packaging and boundaries.** D-Bus activation file and hardened systemd
   user unit are correct, `@ONLY`-substituted, and staged-install-verified
   (placeholder scan + installed consumer link/run, all inside the build
   tree). Boundary poison covers persistence/logging and public-header
   Wayland escape. Shared-registry edits (`src/CMakeLists.txt`,
   `tests/CMakeLists.txt`, `mkdocs.yml`, `adr/index.md`,
   `module-boundaries.md`) are purely additive. No JSON changed. Docs are
   truthful except the consent claim behind P1-1; no UI/persistence/applet
   claims are made. Source shape: 1802 files, exit 0, only two pre-existing
   out-of-lane warnings. AGENT markers present and accurate.

## Commands executed (all from the worktree unless noted)

| Command | Exit | Result |
| --- | --- | --- |
| `git rev-parse HEAD`; `git status --porcelain` (before and after) | 0 | `405577cc…91056`; empty both times |
| Debug configure (exact brief recipe, `-B <ROOT>/debug`) | 0 | generated |
| Release configure (exact brief recipe, `-B <ROOT>/release`) | 0 | generated |
| `cmake --build <ROOT>/debug --parallel 3 --target <14 focused targets>` | 0 | 120/120 steps |
| `ctest --test-dir <ROOT>/debug -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error` | 0 | 12/12 passed |
| `cmake --build <ROOT>/release --parallel 3 --target <same 14 targets>` | 0 | 120/120 steps |
| `ctest --test-dir <ROOT>/release -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error` | 0 | 12/12 passed |
| `./tools/validate-docs` | 0 | 118 Markdown documents and navigation validated |
| `…/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | 0 | built |
| `./tools/check-source-shape` | 0 | 1802 files; two pre-existing out-of-lane warnings |
| `git diff --check` | 0 | clean |
| `python3 -m json.tool` on changed JSON | n/a | no JSON changed (verified via `git diff --name-only`) |
| `diff`/`sha256sum` pinned vs installed `ext-data-control-v1.xml` | 0 | byte-identical (`293deba5…b7fb`) |
| Scratch `optin_repro` (schema default resolution, `<ROOT>/scratch`) | 2 (v1 version probe) | v2 resolves `true` from SystemDefaults; host predicate true |
| Scratch `cache_repro` (request-cache ceiling) | 0 | Busy at id 65, permanent for fresh ids |
| Scratch `offer_flood_repro` (200,001-MIME hostile offer) | 0 | wire buffer overflow, adapter fails closed, no capture |

No gate was skipped or inferred. `tests/session` rows, host buses, hardware,
uinput, and network were not touched; all runtime roots were under the build
tree.

## Verdict

The implementation quality is high — the wire codec, lineage fencing, adapter
fd/lifetime discipline, fail-closed gating scaffolding, packaging, and test
evidence all hold up under attack, and the Debug/Release 12/12 claims
reproduce exactly. Two defects block integration: the shipped product captures
clipboard selections on first start without an explicit user opt-in (P1-1,
against ADR-0031 and the architecture page, with the precise two-part repair
named above), and the remembered-request cache's missing eviction permanently
walls off any caller after 64 mutations (P2-1). ACCEPT requires P0 = P1 = P2 =
0; this candidate has P1 = 1 and P2 = 1.

VERDICT REJECT P0/P1/P2/P3=0/1/1/3
