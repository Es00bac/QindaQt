# S3 WUXGA interaction repeats red; executable lane released

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T09:41:51-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: repeated WUXGA blocker; executable work stopped and lane released

The correctly gated dual-output notification transfer passed three consecutive
runs, each with a fresh green package fixture. The first run,
`e1f4278626e43aa099b8a17079272b4b`, proves post-selector ordered authority
`[WL-1, WL-0]` with priorities `[1, 2]`, an active notification surface and
capture on `WL-1`, all three host-reachability flags false, 237332 KiB resident
PSS below the 1048576 KiB ceiling, bounded cleanup, and no survivor.

The registered package plus four-row matrix then exited 8 with package 1/1 and
rows 3/4. The 1440p125, 1080p150, and dual rows passed. WUXGA result
`e6fac14ae33c556787dc21eaef863802` failed after otherwise exact readiness:
compositor/services/apps/input/output/dock were authoritative, but the
interaction helper returned 8 and `session-interaction.log` says
`notification center did not map on the private seat`. No final envelope or
capture was published. The failure is preserved and not relabeled.

The one Program-Manager-authorized gated WUXGA classification replay repeated
the same causal failure at result `60110c05e8b5da9feae9fba26daaee11`:
CTest exit 8, interaction return 8, requested output `WL-0`, exact message
`notification center did not map on the private seat`. The package fixture again
passed. No source edit was made. Fresh host inspection finds no owned KWin,
Weston, QindaQt, or CTest survivor.

Per instruction, executable work is stopped. I terminally release the
compiler/CTest/private-bus/private-runtime lane now. The mutable Network staging
repair remains unit/build/focused-green but is not frozen as a candidate because
the required four-row acceptance set is not green. Program Manager or an
independent reviewer must classify the preserved repeated WUXGA evidence before
any further repair/rerun direction.
