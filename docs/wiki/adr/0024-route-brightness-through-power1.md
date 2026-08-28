# ADR-0024: Route internal brightness through a Power1 provider

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Power platform service and Display consumers

## Context

KWin owns adaptive brightness and the external-brightness Wayland protocol,
while safe device mutation belongs to logind. KWin 6.6.5 matches internal
brightness devices by internal classification without EDID disambiguation, so
ambiguous multi-panel rigs must not be guessed.

## Decision

Place a cohesive backlight provider inside `Power1`. It registers a protocol
device only for exactly one eligible backlight and exactly one connected LVDS,
eDP, or DSI connector. Ambiguity registers nothing. The provider reads sysfs
only, applies through logind `SetBrightness`, and commits externally observed
changes back to KWin without adding an adaptive loop.

A pure `brightness_model` composes public Power and later Display-client
values. Power code never links Display implementation modules. DDC/CI remains
honestly unavailable until its reserved later decision.

## Consequences

- KWin remains the sole adaptive authority.
- eDP+DSI and every other multi-internal topology fail closed.
- External hardware changes remain observable instead of being overwritten.
- Physical hardware and DDC/CI still require separate qualification.
