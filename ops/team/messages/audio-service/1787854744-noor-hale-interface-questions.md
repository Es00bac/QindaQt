# Audio1 coordination: ADR reservation and future consumer facade

- **Timestamp:** 2026-08-27T12:19:04-06:00
- **From:** Noor Hale, Audio1 backend owner
- **To:** manager/integrator, future native-app route owner, and future shell
  audio-applet owner
- **Safe to continue:** yes; neither answer changes provider internals or path
  ownership

## Q1 — reserve ADR-0013 for Audio1's Qt/GLib boundary

The inspected base ends at ADR-0011, while Samira's accepted plan records
ADR-0012 as occupied by the active Settings1 candidate. I propose ADR-0013 for
Audio1: the D-Bus/service publication boundary stays on Qt's main thread; one
dedicated thread with its own `GMainContext` exclusively owns `WpCore`, object
managers, API plugins, and every other `GObject`; only immutable bounded Audio1
values and operation outcomes cross queued boundaries.

Please confirm the allocation or route a different exact number before
integration. The provider can implement against the decision while keeping the
shared ADR index/MkDocs edit additive.

## Q2 — ratify the future Settings route and shell facade

Audio1 will expose a C++ `AudioClient` only. Its public values keep exact
owner/epoch/revision, domain lifecycle, typed `AudioCapabilities`, typed
operation state/results, and opaque serial+epoch handles. It will not expose
QDBus objects, WirePlumber IDs/GObjects, generic capability strings, QML types,
or the future shared `ServiceAvailability` SDK value.

Proposed native-app boundary:

- stable Settings Center route ID `audio`;
- one domain view model maps Audio1 lifecycle/reason codes to localized
  Starting/Ready/Unavailable/Degraded content;
- the page may show outputs, inputs, streams, defaults, normalized level/mute,
  and busy/error state, and may invoke the four typed client operations;
- route/AppShell code never fabricates devices or flattens uncertain mutation
  results to success.

Proposed shell boundary:

- one purpose-specific facade exposes only the current default output's bounded
  display label, known level, mute, lifecycle/busy state, and requests for level
  and mute plus `openAudioSettings()`;
- detailed device/default/stream selection remains in the Settings page unless
  a later shell owner deliberately extends the facade;
- applet QML receives no raw snapshot, handle, stream inventory, service client,
  or D-Bus authority.

Please answer on a new board record if route identity or facade authority needs
to differ. No page, applet, shared route registry, shell registry, or manifest
is part of the current Audio1 backend slice.
