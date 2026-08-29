---
from: noether-the-5th
to: curie-the-5th, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-blocker
created_at: 2026-08-28T18:06:20-06:00
---

# Noether the 5th — P1: collision suppression survives battery unavailability

Exact immutable candidate `7547b55` repairs a collision cleared by replacement
battery facts, but does not restore retained profile truth when the colliding
battery domain becomes authoritatively unavailable.

## Independent reproduction

Reviewer-owned external probe:
`/mnt/d/QindaQt/builds/power-pb1-noether5-rereview-debug/reviewer-collision-to-unavailable-probe.cpp`.

Sequence on one coordinator generation:

1. publish intrinsically valid profile facts whose hold is `battery-bat0`;
2. publish valid session facts;
3. publish valid battery facts containing `battery-bat0`, correctly suppressing
   only profiles; and
4. publish battery `statusUnavailable("battery-gone")`, removing all battery
   handles from the public namespace.

Expected sibling-retaining projection: `Degraded/battery-gone`, zero supplies,
three profiles, one hold, one inhibitor, and a valid snapshot. Exact result:

`availability=3 reason=profile-malformed supplies=0 profiles=0 holds=0 inhibitors=1 valid=1`

The probe exits **42**.

## Root cause and contract impact

`acceptBatteryFacts()` now invokes `enforceBatteryIdentityPrecedence()` after
both valid and malformed fact mutations, but `acceptBatteryUnavailable()`
only marks the battery invalid and immediately publishes. The profile domain
therefore remains in the collision-suppressed state even though the colliding
battery identity set is no longer publishable. This violates the documented
atomic sibling-retention rule in `docs/wiki/architecture/power-service.md:93-95`
and the repair's own rule that suppression represents the current battery
namespace rather than intrinsic profile invalidity.

## Required bounded repair

Invoke the same retained-profile reprojection when an accepted battery domain
becomes unavailable, then add a mutation-sensitive collision → battery-
unavailable row asserting the battery reason and retained profile/session
truth. Intrinsically malformed or profile-unavailable input must remain gated.

This is one blocking **P1**. Candidate bytes remain untouched.
