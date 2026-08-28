<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Display1 D3 client tests

These five QtTest binaries exercise the production Display1 transport, Client,
and Coordinator. Four use an injected deterministic transport so hostile and
inline callbacks are controllable. One composes the real resident service and
Qt D-Bus transport on a disposable private bus.

| Row | Oracle | Contract |
| --- | --- | --- |
| `qindaqt.display-client-lineage` | fake transport | owner replacement, late/duplicate replies, operation identity, announced epoch, revision regression, and same-revision topology-hybrid rejection |
| `qindaqt.display-client-publication` | fake transport | atomic publication, exact duplicate silence, invalidation coalescing, unavailable LKG removal, empty-owner behavior, and hostile value validation |
| `qindaqt.display-client-operations` | fake transport | deferred completion, timeout without replay, exactly-once stop/start and cancel behavior, monotonic request ids, and typed local rejection |
| `qindaqt.display-client-coordinator` | fake transport | server-projected confirmation readiness, closed outcomes, lineage loss, uncertain cancel resolution, no-op, confirmation point-of-no-return, and non-preemptive rescue timing |
| `qindaqt.display-client-private-bus` | real resident + Qt transport | absent owner/activation, exact-owner readiness, revision invalidation, typed stage/preview/confirm, owner replacement, stale candidate rejection, and teardown |

`FakeDisplayTransport` is the public injection seam, not a duplicate Client.
It records complete requests and lets each case deliver wrong-owner, wrong-kind,
wrong-transaction, late, duplicate, malformed, and inline replies. The private
row launches its own `dbus-daemon`, strips inherited host display/session
addresses from the daemon environment, and connects every participant by the
returned address. No test opens a display, touches the host session bus, or
claims compositor output-management convergence.

Run the focused boundary with:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS -u XDG_RUNTIME_DIR \
  ctest --test-dir build/dev --output-on-failure \
  -R '^qindaqt\.display-client-'
```

Every case must be mutation-sensitive: assertions require one exact outcome,
never an either/or terminal state. The private-bus row is serial and must leave
no daemon or `qindaqt-display-client-private-bus-*` root behind.
