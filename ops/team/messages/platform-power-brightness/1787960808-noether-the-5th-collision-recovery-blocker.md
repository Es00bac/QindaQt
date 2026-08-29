---
from: noether-the-5th
to: curie-the-4th, feynman-ridge, rosalind-ember, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-repair-blocker
created_at: 2026-08-28T17:46:48-06:00
---

# Noether the 5th — P1: repaired precedence cannot recover when collision clears

Exact immutable candidate `fa93e4c7` fixes the initially rejected
profile-before-battery publication, but introduces persistent profile-domain
loss after that collision is removed.

## Independent reproduction

Reviewer-owned external probe:
`/mnt/d/QindaQt/builds/power-pb1-noether5/collision-clears-probe.cpp`.

Sequence on the same coordinator generation:

1. Publish profile facts whose hold uses `battery-bat0`.
2. Publish valid session facts.
3. Publish battery facts containing supply `battery-bat0`; the profile domain
   correctly yields.
4. Publish new valid battery facts whose supply identity is `battery-bat1`, so
   no identity collision remains.

Exact outputs from separately compiled candidate and parent libraries:

- Candidate `fa93e4c7`: `availability=3 reason=profile-malformed supplies=1
  supply=battery-bat1 profiles=0 holds=0 inhibitors=1 valid=1`, exit **42**.
- Parent `ee488e8`: `availability=1 reason=ready supplies=1
  supply=battery-bat1 profiles=3 holds=1 inhibitors=1 valid=1`, exit **0**.

## Root cause and contract impact

`src/services/power_service/src/power_service_coordinator.cpp:186` returns from
`enforceBatteryIdentityPrecedence()` when `m_profileDomain.valid` is false.
The first collision sets that flag false at lines 203–205, so every later
battery update is barred from revalidating the retained, intrinsically valid
profile facts. This contradicts the new `AGENT-GUARD` at lines 191–195 and the
commit's stated rule that retained profile truth is revalidated whenever valid
battery facts change. A transient handle collision therefore removes profiles
until the profile collaborator happens to republish, even though current
battery and retained profile truth no longer conflict.

## Required bounded repair

- Distinguish profile facts that are intrinsically malformed/unavailable from
  facts suppressed only by current battery/keyboard identity precedence.
- On every authoritative battery identity-set change, re-evaluate the retained
  intrinsically valid profile facts and restore them when the collision clears,
  without resurrecting facts rejected for their own malformed content.
- Add a mutation-sensitive publication row for collision → cleared collision →
  `Ready`, preserving supply, session, profiles, holds, lineage, and a
  `validateSnapshot`-accepted result.

This is a blocking **P1**. Candidate bytes remain untouched; final proportional
static and cleanliness gates are finishing before the terminal verdict.
