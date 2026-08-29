# Display D1 shared-registry checkpoint

- **Timestamp:** 2026-08-27T17:57:40-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Manager/router and owners of shared build/documentation registries
- **Base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **State:** uncommitted source/docs checkpoint; compiler not used

The authorized shared edits are additive only. Existing targets, navigation,
ADRs, ownership rows, and evidence wording are preserved. Exact diff:

```diff
diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
@@
 add_subdirectory(services/settings_client)
+add_subdirectory(services/display_protocol)
+add_subdirectory(services/display_identity)
+add_subdirectory(services/display_topology)
+add_subdirectory(services/display_transaction)
 add_subdirectory(services/audio_protocol)
diff --git a/tests/CMakeLists.txt b/tests/CMakeLists.txt
@@
 add_subdirectory(services/settings_client)
+add_subdirectory(services/display_protocol)
+add_subdirectory(services/display_identity)
+add_subdirectory(services/display_topology)
+add_subdirectory(services/display_transaction)
 add_subdirectory(services/audio_protocol)
diff --git a/mkdocs.yml b/mkdocs.yml
@@ Architecture
       - Settings model: architecture/settings-service.md
+      - Display service: architecture/display-service.md
       - Audio service: architecture/audio-service.md
@@ Reference
       - Settings1 protocol 1: reference/settings1-v1.md
+      - Display1 version 1: reference/display1-v1.md
       - Audio1 version 1: reference/audio1-v1.md
@@ Decisions
       - "ADR-0014: Confined WirePlumber worker": adr/0014-confine-wireplumber-to-glib-worker.md
+      - "ADR-0015: Display1 transaction authority": adr/0015-display1-transaction-authority.md
+      - "ADR-0016: Persistent output identity": adr/0016-persistent-output-identity.md
diff --git a/docs/wiki/adr/index.md b/docs/wiki/adr/index.md
@@
 | [ADR-0014](0014-confine-wireplumber-to-glib-worker.md) | Accepted | Confine libwireplumber/GObject ownership to a dedicated GLib worker |
+| [ADR-0015](0015-display1-transaction-authority.md) | Accepted | Make Display1 the QindaQt display-transaction authority while KWin owns live state and restore |
+| [ADR-0016](0016-persistent-output-identity.md) | Accepted | Derive privacy-preserving persistent output identities with explicit ambiguity |
diff --git a/docs/wiki/index.md b/docs/wiki/index.md
@@ Start here
 - [Audio service](architecture/audio-service.md) records the typed Audio1
   model/client/service boundary, confined WirePlumber adapter, activation, and
   isolated-runtime qualification.
+- [Display service](architecture/display-service.md) records the pure Display1
+  values, identity/topology boundaries, and deterministic transaction model;
+  its runtime service and compositor adapter are later milestones.
@@
 - [Audio1 protocol version 1](reference/audio1-v1.md) documents the fixed
   device/stream snapshot, handle lineage, operation results, and bounds.
+- [Display1 version 1](reference/display1-v1.md) documents display value bounds,
+  identity/registry rules, topology projection, codecs, and transaction states.
diff --git a/docs/wiki/architecture/overview.md b/docs/wiki/architecture/overview.md
@@ Runtime shape
 | Audio service | Bounded typed PipeWire graph snapshots and validated controls through the running WirePlumber authority | Samples, devices, WirePlumber policy, PipeWire configuration, or UI |
+| Display foundation (D1) | Bounded values, privacy-preserving identity/registry, topology validation, and injected-port preview/revert model | A runtime service, KWin/Wayland mutation, Settings persistence, timers, or UI |
@@ Interaction boundaries
 - `QindaQt.Applets 1.0` for manifest-defined extensions.
+
+`org.qindaqt.Display1` is reserved by the version-1 pure value model but is not
+yet a cross-process runtime. The future focused service will coordinate
+QindaQt display transactions through KWin's public protocol while KWin remains
+live/restore authority. See [Display service](display-service.md).
@@ Decisions
 The Audio1 Qt/GLib ownership boundary is recorded in
 [ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md).
+Display transaction authority and persistent identity are recorded in
+[ADR-0015](../adr/0015-display1-transaction-authority.md) and
+[ADR-0016](../adr/0016-persistent-output-identity.md).
diff --git a/docs/wiki/architecture/module-boundaries.md b/docs/wiki/architecture/module-boundaries.md
@@ Source ownership
 | `src/services/settings_client` | Activation, exact-owner/epoch asynchronous snapshots and writes, timeout/uncertainty recovery, and DND-scoped state projection | Settings protocol plus Qt Core/DBus; never service persistence, shell presentation, or settings files |
+| `src/services/display_protocol` | Display1 versioned values, hostile-input limits, semantic validation, canonical byte codec, and QtDBus value serialization | Qt Core and serialization-only Qt DBus; never connection/name/service/XML/client/platform state |
+| `src/services/display_identity` | Pure privacy-preserving stable-ID resolution plus schema-v2 registry values and v1 migration | Qt Core only; never EDID acquisition, Settings persistence, runtime UUID authority, or logs of private material |
+| `src/services/display_topology` | Pure candidate validation, normalization, logical geometry, mirror projection, canonical fingerprint, diff, and no-op | Public display protocol plus Qt Core; never KWin, Wayland, stored preferences, or mutation |
+| `src/services/display_transaction` | Pure one-transaction state machine, journal value/codec, rollback/hotplug/recovery truth, and injected clock/port seams | Public display protocol/topology plus Qt Core; never real clocks/timers, files, D-Bus/Wayland, lock/logind, or QObject providers |
@@ Dependency direction
   See [Audio service](audio-service.md) and
   [ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md).
+- Display consumers will depend on a typed Display1 client, not these service
+  implementation modules. D1's dependency direction is protocol → identity or
+  topology → transaction; identity is independent of topology/transaction.
+  KWin remains live/restore authority, Settings owns later registry/policy
+  persistence, and shell geometry never waits for Display1. See
+  [Display service](display-service.md),
+  [ADR-0015](../adr/0015-display1-transaction-authority.md), and
+  [ADR-0016](../adr/0016-persistent-output-identity.md).
diff --git a/docs/wiki/development/testing-harness.md b/docs/wiki/development/testing-harness.md
@@
+## D1 deterministic display model
+
+The focused `qindaqt.display-*` unit rows cover bounded protocol/codecs,
+privacy-preserving identity and registry migration, topology geometry and the
+accepted fractional rounding table, plus fake-clock/port transaction,
+hotplug, rollback and journal recovery. The exact row-to-contract mapping is in
+[Display1 version 1](../reference/display1-v1.md#deterministic-acceptance-matrix).
+
+These rows are **deterministic model evidence (`Q-det`)**. They use pure Qt
+values and fakes and open no display. They do not prove the pinned KDE output
+protocol accepted a configuration, mirror visibility in `wl_output`, service
+activation/restart behavior, or physical DRM/GPU/monitor/lid/suspend results.
+D0/D2 own nested protocol/service proof; D8 owns isolated hardware proof. A D1
+handoff must not upgrade unit success into either claim.
+
 ## Required display matrix
```

The two owning pages and ADR files are new primary documents rather than
shared-list edits. `git diff --check` and source-shape pass at this checkpoint.
No configure/build/test claim is made.

