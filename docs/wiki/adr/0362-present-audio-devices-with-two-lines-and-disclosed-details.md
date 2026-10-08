# ADR-0362: Present Audio devices with two lines and disclosed details

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** Settings Audio route
- **Supersedes:** [ADR-0288](0288-per-device-latency-offsets-and-a-compact-audio-page.md), Settings and compact-page presentation clauses only
- **Superseded by:** None

## Context

The owner confirms that devices now appear in Audio Settings, but the page
still wastes space. ADR-0288 reserves a third line for latency and individual
channel controls on each device. Actual unchanged-source captures and a native
layout control measure 74 logical pixels between device faders. Common speaker
and microphone controls should fit together in a small window while preserving
the desktop's full routing and adjustment capabilities.

The [Audio Settings page](../apps/audio-settings.md) owns this presentation.
Its public Audio1 values, admission, operation lifecycle, latency persistence
and gain/routing behavior continue to follow their existing contracts.

## Decision

Use two compact lines for common device controls: device name/state and default
selection, followed by volume, mute and a device-specific **Details** action.
Details reveals reported latency offset/reset and the existing channel
disclosure. The channel disclosure instantiates its individual faders only
when opened. Keep known read-only offsets inspectable and unknown offsets
absent; disclosure does not manufacture an audio operation.

Details is keyboard focusable and names its device for assistive technology.
Normal Tab traversal reaches it and its expanded controls. The Settings host
nominates it as an action-entry target only when it exposes an admitted edit,
so read-only details do not precede another device's admitted action. Closing
Details closes the channel editor and returns focus to the disclosure.

Disclosure is local presentation state, with no persisted setting. Preserve it
through snapshots of the same device and reset it when a replacement device
takes the row's list index. Retain vertical scrolling, keyboard paging and
focus reveal for all expanded controls. Keep the toolkit's larger theme/touch
targets; do not shrink interaction height to manufacture a passing layout.

## Consequences

- More speaker and microphone controls fit together before scrolling. Advanced
  latency/channel adjustments require opening Details.
- This supersedes ADR-0288's always-present offset/channel line and its original
  direct keyboard path. Its service mechanism, schema, persistence, readback,
  bounds and bus-delay decisions remain accepted.
- Focused native fixtures require positive device spacing at most 64 logical
  pixels and sliders at least 22 logical pixels under the shipped dark theme,
  with both outputs and the first microphone fader visible at 420×320. Repeat
  at normal and 2× scale and inspect actual captures.
- Keyboard and accessibility checks retain latency/reset/channel dispatch,
  known/read-only/unknown truth, same-device snapshot stability, replacement
  collapse, focus restoration and no writes from disclosure. Existing page,
  console, wheel, routing and boundary checks remain required.
- Source and private rendering evidence do not qualify the installed desktop,
  live audio devices, acoustic alignment, hardware controls or the full mixer.

## Revisit when

- Owner feedback still finds device discovery or common controls too crowded.
- A supported theme, large-text setting or touch target makes the small-window
  controls overlap or prevents keyboard access.
- The device inventory gains a stable model identity that replaces the current
  count-based row lifetime.
