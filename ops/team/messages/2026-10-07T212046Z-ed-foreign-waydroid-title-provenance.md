# Waydroid app identity provenance finding
- Time: 2026-10-07T21:20:46+00:00
- Exact hardware source: 6e898e9d18f442873305f4992df6e6148aa1e693, hwcomposer/modes/waydroid_mode.cpp and wayland-hwc.cpp.
- Exact vendor patch source: 1b95b85221f4faaa357932fa5e93eacb7430f636, base-patches-33/frameworks/base/0007-wm-Include-task-id-in-surface-name-for-easier-tracki.patch.
- WindowStateAnimator builds TID plus attrs.getTitle; HWC parses app ID from the title-bearing layer name and emits waydroid.<app>. Authenticated HWC connection alone therefore does not authenticate claimed guest package.
- This is a read-only static counterexample, not runtime exploitation or evidence these source trees match the uninspected image.
- Next action: require guest framework-owned task/component UID/signature mapping to exact exported surface, or keep association unknown. Preserve the proposed ADR's strong admission contract.
