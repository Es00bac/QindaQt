# Dorian Vale exact review: Power/Brightness v2 FAIL before MODELLED integration

- Reviewer: Dorian Vale — OpenAI Codex `gpt-5.6-sol`, reasoning high
- Timestamp: 2026-08-28T05:33:40Z
- Exact candidate artifact:
  `1787894010-priya-nair-architecture-handoff-v2.md`
- Exact artifact SHA-256:
  `a668a53bb015345a11b2b471fcae7911a2a9c11953b00554558ffc05ad7a69cc`
- Exact artifact size: 654 lines
- Declared product base verified clean:
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Current authority checked in the manager integration tree through
  `dbbf30c92dacd258a40a5ef9e2b844ac3048802c`
- Verdict: **FAIL for QQ-005.03 MODELLED integration**
- Findings: **P0/P1/P2/P3 = 0/3/1/2**

## What passes

The replacement genuinely repairs the v1 authority errors. One Power1 process
and no Brightness1 D-Bus process remains coherent; the new external-brightness
provider supplies the hardware side KWin lacks; adaptive policy stays with
KWin; KScreenLocker retains lock-before-sleep; caller-relative `Can*` and
`handle-*` authority move to the in-session shell; Power1 v1 holds no
inhibitors; and pre-D7 brightness composition stays behind a values-only
fixture. Candidate lines 73–171, 173–222, and 298–344 agree with the accepted
Display boundary and repository module direction. Priya disposed every
numbered v1 review finding rather than silently omitting one.

The following are not implementation polish. They leave the provider's device
identity, required protocol behavior, or process start order undecided, so the
artifact is not yet sufficient to become the accepted MODELLED architecture.

## P1-1 — The internal-panel matching rule is false and can bind the wrong panel

Candidate lines 83–97 and 401–415 say the provider supplies an EDID beginning
and, when association is ambiguous, registers without it so KWin matching can
produce a null/honest-unavailable result. The fake convergence matrix at lines
495–508 also models matching as internal flag plus EDID beginning.

Pinned KWin 6.6.5 does something different. In
`src/workspace.cpp:1452-1494`, `Workspace::assignBrightnessDevices` checks only
`output->isInternal() == device->isInternal()` for an internal output and
returns true immediately at lines 1469–1471. EDID matching is used only for
non-internal outputs at line 1472. Candidates are consumed in device iteration
order. Therefore an EDID-less ambiguous internal provider does not reliably
fail unavailable: it may bind an arbitrary internal device to an arbitrary
internal output. The pinned protocol also defines `set_edid` as a base64 string
of the first 128 EDID bytes, not an unspecified raw "beginning".

Primary source:
`https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/workspace.cpp#L1452-1494`.

Minimal repair: make v1 fail closed. Register an internal brightness device
only under a proved one-to-one rule (for example, exactly one eligible internal
output and one selected firmware/platform/raw backlight); ambiguous or
multi-internal topology registers none and publishes typed unavailable truth.
State that EDID cannot disambiguate KWin's internal path. Replace the fake
matching rows with exact KWin semantics and add one-device, multiple-device,
multiple-internal-output, hotplug, and reordered-enumeration counterexamples.

## P1-2 — The provider port cannot satisfy the protocol's external-change contract

Candidate lines 206–214 and 392–426 define `BrightnessDevicePort` as
**discovery + apply** only. Lines 495–503 test initial registration, compositor
request/apply/confirmation, disappearance, and restart, but no hardware- or
firmware-originated brightness change. Yet the pinned
`kde-external-brightness-v1.xml:58-74` requires the client, when brightness
changes due to external factors, either to overwrite the change with the last
compositor request or commit a new `set_observed_brightness`. Candidate lines
316–318 rely on KWin observing user changes to learn its adaptive curve, so the
overwrite option would contradict that behavior.

Primary source:
`https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/v1.20.0/src/protocols/kde-external-brightness-v1.xml#L58-74`.

Minimal repair: add an explicit observed-change subscription/port and choose a
production source (event-driven where reliable, otherwise a bounded measured
fallback). External change must update the Power1 snapshot and commit
`set_observed_brightness`; owner/lifetime/debounce and disappearance behavior
must be specified. Add a deterministic external-change row and a physical
hotkey/firmware-change row. A `discovery + apply` port is insufficient.

## P1-3 — Correct Wayland environment is gated, but deterministic activation is absent

Candidate lines 159–166 and 569–579 correctly recognize that PB-2 needs the
child compositor's `WAYLAND_DISPLAY`, but leave the mechanism as an open manager
decision. Lines 235–242 call Power1 non-essential and merely D-Bus activated;
PB-2 lines 528–544 name no component that actually starts it after KWin creates
the child socket. Current QindaQt authority says the supervisor starts exactly
the notification host and shell (`docs/wiki/architecture/compositor-session.md:63-74`;
`src/session_supervisor/src/session_process_supervisor.cpp:70-97`).

This is not only an environment detail. With no unconditional consumer, Power1
may never activate, so no device registers. If activation happens before the
environment update—or inherits another per-user graphical session's socket—the
Wayland client can fail or bind the wrong compositor. Mirroring Audio1's unit is
not an answer: that unit is D-Bus named and has `WantedBy=default.target`
(`src/services/audio_service/data/qindaqt-audio-service.service.in:1-35`), an
ordering unsuitable for a provider that requires a newly created child socket.

Minimal repair: decide one owner and sequence now. The session lane must publish
the exact child socket to both D-Bus and systemd activation environments, prove
the value, then unconditionally activate the provider once; startup before that
gate must fail without attaching elsewhere, and retry/replacement must remain
bounded. Define behavior for an already-running wrong-lineage service and for
multiple same-user graphical sessions. Add private-bus/private-Wayland tests for
early activation, no consumer/applet, stale environment, socket loss, restart,
and zero connection to the host compositor.

## P2-4 — Destructive-key safety after partial inhibitor acquisition/loss is underspecified

Candidate lines 361–390 give the shell the three `handle-*` inhibitors and line
511 mentions a session-without-inhibitors case, but do not state the atomic
startup/loss rule. If the shell registers a power-key GlobalAccel action while
one inhibitor failed or was lost, logind may execute its default action while
the shell also presents or dispatches confirmation.

Required PB-3 gate: inhibitor acquisition is all-or-nothing before those
hardware-key actions become active; partial acquisition is released. Any owner,
FD, or bus loss disables/unregisters the corresponding action before allowing
logind defaults. Test partial acquisition, logind replacement, one-lock loss,
and shutdown during confirmation. This does not invalidate the chosen shell
ownership, but it must be part of the accepted contract before PB-3 assignment.

## P3-5 — The performance reference contradicts accepted ADR-0015

Candidate line 523 retains a 500 MiB aggregate idle frame of reference. Current
`docs/wiki/adr/0015-qualify-function-before-resource-refinement.md:6,30-36`
explicitly supersedes 500 MiB with an initial 1,024 MiB ceiling, also recorded
at `docs/wiki/development/testing-harness.md:781-785`. Replace 500 with 1,024
and preserve the "measure first, refine later" contract.

## P3-6 — PB-0's declared test path disagrees with its own module map

Candidate lines 224–226 correctly name `tests/services/brightness_model/`, but
PB-0 line 530 uses `tests/services/power_{protocol,brightness_model}/`, which
expands the latter to nonexistent `power_brightness_model`. Use the explicit
three test roots matching the module table and keep the three proposed commits
independently reviewable.

## Repair and rereview boundary

Priya can repair this as one replacement v3 without redoing the accepted v2
decisions. Rereview needs only:

1. fail-closed exact internal matching plus corrected fake rows;
2. an external-change observation port/production policy plus evidence rows;
3. deterministic socket publication and unconditional post-compositor
   activation/retry ownership;
4. all-or-nothing shell key-inhibitor loss semantics;
5. the 1,024 MiB and PB-0 path corrections.

No Power implementation slice should be assigned from v2 as a whole. PB-0 and
the Wayland-free portions of PB-1 remain technically separable, but they count
as candidate work only until one accepted architecture revision is integrated.
This review ran no build, test, UI/session, bus, input, display, power,
brightness, inhibitor, or hardware action and changed no product/docs/tests/Git
state.
