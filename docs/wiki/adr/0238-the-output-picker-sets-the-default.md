# ADR-0238: The audio applet's device picker sets the system default

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** shell/audio_applet
- **Supersedes:** None
- **Superseded by:** None

## Context

The audio applet's output and input pickers chose which device each band's
fader and mute addressed. They did not change the system default, and
[Audio applet](../shell/audio-applet.md) stated that as a contract, adding that
the picker "must not appear to". `AudioDeviceRow.qml` repeated it.

The second half of that contract did not hold in practice, and the way it
failed is worth recording because nothing in the code was broken.

A user connected a Bluetooth speaker, opened the applet, and picked it from the
output list. The picker showed the speaker. Sound continued to come out of the
laptop. Nothing in the applet, the panel, or Settings said why.

Underneath, PipeWire had two keys that had diverged:

```
default.configured.audio.sink = bluez_output.41_42_A5_2B_14_19.1
default.audio.sink            = alsa_output.pci-0000_04_00.6.analog-stereo
```

The configured default named a *different, absent* speaker chosen at some
earlier point. WirePlumber could not resolve it, so the effective default fell
back to the built-in analog output and stayed there. The applet offered the
one control a user would reach for, and that control could not address the
problem, because the applet had no set-default intent at all.

So the defect was not the routing stack, which worked correctly the moment the
configured default named a present device. The defect was an affordance: a list
of output devices at the top of an audio applet is read as "send sound here" by
everyone who sees it, and writing a contract saying it must not appear that way
does not change how it appears.

## Decision

Picking a device in the applet's output or input picker makes that device the
system default **and** points the band at it. These were two ideas in the code
and were never two to a user.

`AudioAppletController::requestDefault(serial)` joins the applet's intent
surface. It is the only applet intent that changes routing rather than levels;
the surface remains closed to everything else. It is gated on the published
`Capability::SetDefault` and on the applet's control grant, refuses a serial
that has left the graph, and is a no-op that reports success when the device is
already the default, because the picker dispatches on every activation
including a re-pick of the current entry.

It does not use the `beginRequest()` path that volume and mute share. That path
exists to clamp a continuous value and to replace a queued one mid-drag; a
default has no value to clamp, and re-sending it while one is in flight would
only race the service for the same outcome.

## Consequences

- The most common audio action on a desktop — "play sound through that device"
  — is available where users already look for it, instead of only in Settings.
- The applet can correct a configured default that names an absent device. It
  is now the recovery path for the state that produced this ADR.
- The applet is no longer purely a level surface. Any future claim that it
  "never changes routing" is false and must not be written back into the
  contract.
- Settings keeps its own default-device control. Two routes to one outcome is
  intended here; the Settings route carries per-channel volumes, stream moves
  and virtual devices that the applet deliberately does not.
- A QML row covers this, not only a controller row. A controller test would
  have passed against the defect, because the controller was never the part
  that was wrong — the picker simply never called it.

## Revisit when

The applet grows a second routing intent, such as moving individual streams. At
that point the "one routing intent" framing above stops describing the surface
and the boundary needs restating rather than extending by habit.
