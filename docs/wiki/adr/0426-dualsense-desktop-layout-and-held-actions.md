# ADR-0426: DualSense desktop layout and held actions

- Status: Accepted
- Date: 2026-10-20
- Owners: Controller platform and Settings Input
- Amends: [ADR-0347](0347-controller-desktop-integration.md), family defaults and action vocabulary

- Amended by: [ADR-0427](0427-share-programmable-input-actions-and-controller-layers.md), programmable input actions/layers, contact gyro and fullscreen media

## Context

The owner supplied an exact DualSense layout. The existing action vocabulary
cannot express voice commands, a gyro toggle or mouse drags with Meta+Shift.
Using a generic PlayStation default would also change PS3/PS4 behavior. Saved
per-device profiles and transport-independent identity must survive the change.

## Decision

Add the `dualsense` family for SDL's PS5 type, including DualSense Edge, with a
separate editable default profile. Other families retain their defaults. On
load, legacy PlayStation rows named DualSense acquire the new family metadata;
their IDs and saved config remain unchanged. Reset uses factory defaults for
the row's family. Applying the owner's map to remembered devices uses the
existing exact-revision Controllers1 patch boundary.

Add enumerated `voice-command`, `toggle-gyro`, `left-click-meta-shift` and
`right-click-meta-shift` actions without changing the version-1 JSON shape.
Old plugins do not understand the new action IDs; install the updated plugin
before saving them. VoiceButton selects `StartCommand` or `StartDictation`
against the exact current Voice1 owner, finishes on release and cancels on
suppression. One owned capture prevents an overlapping button stealing it.
Idle and Error both admit a fresh hold, as Voice1 specifies; an earlier provider
error must not disable subsequent controller captures. Recovery never invokes
Retry or replays the previous utterance.

Gyro toggle belongs to the runtime, which saves the inverted gyro preference
atomically on the press edge before publishing it and updating matching live
devices. Save failure preserves the previous preference. Held/released and
suppressed buttons cannot repeat it. Unsupported sensors ignore the action.
The existing neutral resume rule still applies.

A private value-only held-input collaborator owns per-token key/button counts.
It orders modifiers before mouse-down and after mouse-up and suppresses duplicate
presses and premature releases. The compositor sink alone converts bindings to
platform codes and emits input. No new service, dependency or shell execution
path is introduced.

## Consequences

The profile store has five family defaults; Settings recognizes DualSense labels
and exposes all new actions. Existing user motion preferences remain intact when
only bindings are patched. Virtual SDL, held-input and private-bus fixtures prove
the layout, gyro persistence, neutral resume, modifier ordering and both voice
modes without requiring physical hardware or another model review.

See [Controller desktop input](../architecture/controller-input.md).
