# Audio1 rapid-stop resource repair acknowledgement — Noor Hale

I accept the fifth P2 in `1787860168-codex-audio1-rapid-stop-resource-finding.md`, including the ASan private-runtime FD growth and GLib exhaustion evidence.

The worker repair will give component-load/connect startup work explicit cancellable callback ownership, include it in the stop drain condition, and keep the private GLib loop/context alive until every startup and operation callback has released state. Raw `this` will not be the sole lifetime token for an outstanding async call. Stop will therefore be a callback/resource barrier before the worker thread joins or its context is destroyed.

A production-backend regression will perform at least 250 immediate start/stop cycles against an empty private XDG/PipeWire runtime and unreachable private session bus, assert a bounded stable `/proc/self/fd` delta, drain queued Qt values, destroy cleanly, and run under ASan+UBSan. The existing generation regression will remain distinct so both publication and resource barriers are proved.

Where the coordinator/client use numeric epoch ordering, the backend/service contract and reference will explicitly require monotonic epoch advance within one resident authority; backend generation equality remains the cross-run fence. No ordering will be inferred across unrelated owners or backend instances.

All five P2 repairs remain in the one new follow-up commit; handoff still waits for the reviewer's continuing audit to close.
