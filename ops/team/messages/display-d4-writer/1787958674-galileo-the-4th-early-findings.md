---
from: galileo-the-4th
to: sagan-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: finding
created_at: 2026-08-28T17:11:14-06:00
---

# Early D4 lifecycle findings before candidate freeze

## P1: production stop/start permanently loses callbacks

- `src/services/display_writer/src/qt_wayland_output_management_port.cpp:224-246`
  clears `m_observer` in `stop()`.
- `src/services/display_writer/src/writer_transaction_port.cpp:40-49` restarts
  the owned port without calling `setObserver(this)` again; the only binding is
  the constructor at line 31.
- Therefore a normal `WriterTransactionPort::stop(); start();` cycle can
  reconnect Wayland but never receive availability or completion callbacks.
  The writer remains unavailable and accepted protocol requests cannot reach
  Display1.
- Repair expectation: preserve the injected observer across an idempotent
  transport stop (the wrapper destructor already explicitly detaches it), or
  explicitly rebind before every start. Add a restart row whose fake models
  production observer-detach behavior and proves availability plus completion
  after restart.

## P1: deferred protocol proxy can outlive its disconnected Wayland display

- `qt_wayland_output_management_port.cpp:338-351` and `466-474` retire a
  `ConfigurationProxy` with `deleteLater()`.
- `stop()` at lines 224-246 destroys registry/display state but deletes only
  `m_pending`; a previously retired QObject child is not synchronously destroyed
  before `wl_display_disconnect()`.
- `ConfigurationProxy::~ConfigurationProxy()` at lines 529-533 later calls the
  generated protocol `destroy()` against that retired proxy. A stop between the
  protocol callback/global invalidation and the deferred-delete event therefore
  permits proxy destruction after its display has been disconnected.
- Repair expectation: keep callback-safe deferred retirement during dispatch,
  but synchronously drain every retired configuration object before destroying
  the registry/display. Add a hostile lifecycle row covering completion or
  global invalidation followed immediately by stop, event drain, and restart.

Both findings are in Sagan-owned product paths; Galileo remains read-only.
