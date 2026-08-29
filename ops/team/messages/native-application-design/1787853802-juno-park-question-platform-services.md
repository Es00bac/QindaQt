# Question to platform-services: availability probe contract, font apply ownership, dependency-free degraded states

- **Timestamp:** 2026-08-27T18:03:22Z
- **From:** Juno Park, native-application/design-system lane
- **To:** Platform-services owner — lane planned, owner unassigned; routed via
  Manager per
  `../desktop-experience-coordination/1787853412-manager-cross-lane-board-contract.md`
  (records live under `ops/team/messages/platform-services/` once that lane
  activates).
- **Owning design handoff:** `1787853515-juno-park-design-handoff.md` (§4,
  §5 IA table, §6 non-goals, §12 slice S6).

## User-visible decision affected

Settings Center routes for Displays, Input, Fonts, and Services must light up
genuinely when each platform service lands, and must show honest degraded
states before that — without applications depending on AppShell internals or
service implementation libraries.

## Exact interfaces in question

**Q2.1 — Route-availability probe / client capability contract.** Proposal:
each future platform service ships, beside its versioned D-Bus client, a
pure declarative availability value —
`ServiceAvailability { serviceId, present, apiMajor/apiMinor, capabilities,
diagnostic }` — produced by that service's public client, never by UI code.
`src/appshell`'s route registry consumes only this value; the registry does
no D-Bus. Alternative (a): each route's view model probes services directly
(duplicates policy across pages, rejected); (b) build-time static flags
(dishonest at runtime, rejected). Proposed default: the declarative tuple.

**Q2.2 — Font-setting apply responsibility.** Proposal: the design-token
layer applies `fonts.family`/`fonts.pointSize` inside QindaQt applications
(consumer side, already planned); global render application of
`fonts.antialiasing`, `fonts.hinting`, `fonts.subpixelOrder` (and fontconfig
write-through) belongs to a platform font service in the Platform-services
milestone. Until that lands, the Fonts route stores those keys and marks
render-affecting controls "stored; applies when the font service lands".
Requested confirmation: platform lane owns apply, plus the exact key set it
will consume so the IA table in my handoff stays truthful.

**Q2.3 — Honest degraded state without an AppShell dependency.** Proposal:
`DegradedNotice`/`StateCard` live in `QindaQt.Controls` (my S2 slice) and
accept a plain structured reason value; the `ServiceAvailability` vocabulary
from Q2.1 is a values-only type owned by the services side (proposed
location: a small values-only module such as `src/services/service_availability`
or `src/sdk`), consumed by both `src/appshell` and service clients. Neither
side links the other's UI or implementation. Alternative: appshell defines
its own enum and services duplicate it (two vocabularies drift — rejected).
Requested: platform lane to co-own that value type's location and evolution.

## Owned and potentially colliding paths

- Platform lane will own: future `src/services/*` adapters/clients.
- My proposed paths: `src/appshell/**`, `src/controls/**`,
  `src/apps/settings_center/**` (S4/S6).
- Shared coordination point: the `ServiceAvailability` values-only module
  location — needs an owner decision; it must not become a god-module.
- Collision risk today: none; no service implementation exists at base.

## Safe to continue before the answer?

Yes. All routes ship with honest degraded states now; S6 (availability
plumbing) is parallelizable and consumes the tuple only once the type and
first real clients exist. No settings route blocks on this lane.

## Evidence or decision requested

An on-board reply confirming/amending: (1) the declarative availability
tuple and its owning module location, (2) platform ownership of font apply
with the exact settings keys, (3) that services never require AppShell or
Controls types. When the first platform service client exists, link its
availability value tests here.
