# ADR-0244: Gate desktop Voice use on confirmed Settings1 opt-in

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Voice consumers and Settings1 client
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0233](0233-own-voice-input-as-a-contract-not-an-implementation.md)
defined services.voiceInput as Off by default and said it decides whether
QindaQt uses a speech provider. The implementation treated that value as a
Settings-page switch only: the shell applet, Settings route and Voice console
each started their Voice1 client without reading it. Starting a Voice1 client
can activate an installed provider through D-Bus.

The provider's own shortcut-armed state is a separate Voice1 value. QindaQt
can withdraw its clients and their actions, but it cannot silently terminate
or control an independently running provider's own shortcuts merely by
disconnecting a client.

## Decision

A small VoiceInputPreferenceGate consumes a scoped public Settings1 client.
It admits desktop Voice use only after a confirmed, exact boolean On for the
current Settings1 owner. The shell, Voice Settings route and console each own
their Voice1 client lifecycle and start it only while the gate admits use.
Off, owner loss/replacement, degraded Settings1 authority, or a missing
baseline stops the client and clears its provider projection. The shell's
manifest voice.read grant remains an additional condition.

A same-owner Settings1 refresh retains the last confirmed On so changing the
independent panel-transcript preference does not interrupt a capture.
SettingsClient::currentOwner() lets the gate compare a retained snapshot
against the current transport owner without changing SettingsClient behavior.
On a new owner, only that owner's new baseline can restore admission.

The Settings route remains available while Voice1 is stopped. It saves the
desktop preferences independently. Applying an explicit Off withdraws that
route's Voice actions immediately while the write is pending; a rejected
write can restore use from the still-confirmed On baseline. A successful
write is projected only after the authoritative snapshot. The route's
two-key draft waits for that refreshed revision before sending its second
key; uncertain operations are never replayed.

The provider's own independently registered shortcuts are outside this
desktop admission contract. The Voice1 enabled field still reports whether
those shortcuts are armed. QindaQt does not claim that desktop Off changes that
field or stops an unrelated provider process.

## Consequences

Opening Settings or the Voice console with default Off does not activate a
provider. The panel shows its unavailable projection and no desktop action is
admissible. A confirmed On connects each allowed desktop consumer live; Off
or owner loss disconnects them live. The panel transcript preference remains
independent of the Voice input preference.

All consumers share the same gate semantics without sharing a Voice1 client or
linking a presentation module into the protocol. The gate borrows its Settings1
client on one thread; the caller keeps that client alive and scopes it to
services.voiceInput.

## Alternatives rejected

- **Use the provider's SetEnabled(false) as the Off implementation.** That
  would activate a default-off provider just to disarm it, change another
  process's own shortcut policy, and conflate provider state with desktop
  consent.
- **Stop only the shell applet.** The Settings route and Voice console would
  still activate and operate Voice1 while the same desktop preference said Off.
- **Treat every Settings1 refresh as loss of consent.** A same-owner transcript
  preference save would interrupt a live capture without any change to Voice
  input.
