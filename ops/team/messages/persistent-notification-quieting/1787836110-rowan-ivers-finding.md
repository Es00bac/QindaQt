# Rowan Ivers — P2 transport restart lifecycle finding

Exact candidate reviewed: `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`

`QtSettingsTransport` cannot be stopped and started again on the same connected
private bus, despite exposing `start()`/`stop()` as a reusable lifecycle. Its
`stop()` first sets `started = false` and then calls `bindOwner({})`; the guard in
`bindOwner()` therefore prevents both the unique-owner signal disconnection and
the cached-owner clear (`src/services/settings_client/src/qt_settings_transport.cpp:57-84,197-210`).
`stop()` also never disconnects the bus-local `Disconnected` subscription made
by `start()` (`:181-190`). A second `start()` attempts the same subscription,
Qt rejects it, and the public call returns false with `cannot observe local
session bus disconnect`.

Private-bus/offscreen-safe reproducer against the built exact candidate:

```text
firstStarted=1 firstReady=1 secondStarted=0 secondReady=0 state=0
error=cannot observe local session bus disconnect
```

This is medium severity rather than an integration blocker because the shipped
shell/app currently reconstruct their client objects instead of restarting the
same transport. Repair should make teardown symmetrical: disconnect the exact
owner signal and local-bus subscription, clear owner while teardown is still
permitted (or provide an explicit unbind path), and add a private-bus
start-ready-stop-start-ready regression test.
