---
name: claude-plasma-free
role: Lane PF planner — QindaQt without Plasma services, and QindaQt's own KWin
provider: Anthropic Claude
model: claude-opus-5-5
status: handoff
feature: Plan (no code) to remove every Plasma service dependency, starting with the SDDM Plasma entry
worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/plasma-free
started_at: 2026-09-29T01:25:32Z
---

# claude-plasma-free

- Role: lane PF of the 2026-09-28 round 2 (plan only, no code changes).
- Status: handoff — plan committed on plan/plasma-free (docs/plans/2026-09-28-plasma-free-qindaqt.md); awaiting the owner's answers to its eight open questions.
- Exact base: `190e49ba` (hub main at claim).
- Branch: `plan/plasma-free`.
- Product authority: `docs/plans/2026-09-28-plasma-free-qindaqt.md` and this record only. No ADR reserved.

## Updates

- 2026-09-29T01:25:32Z: claimed lane PF; worktree created from 190e49ba; read the KWin 6.6.6 CMake options, the overlay kwin and qindaqt-desktop ebuilds, the qindaqt profile, and the live laptop session.
- 2026-09-29T01:31:21Z: material findings — plasma-workspace (and so the SDDM Plasma entry) is pulled only by kscreenlocker (greeter LnF QML + lookandfeel package structure), powerdevil (links libkworkspace) and kde-plasma/sloom-globalmenu; polkit-kde (supervisor) and polkit-gnome (XDG autostart) race each login (polkitd log shows both winners); SDDM greeter falls back from Wayland to X11 every boot (no compositor configured); KWin 6.6.6 has no ext-session-lock-v1 and hard-requires KNightTime.
- 2026-09-29T01:37:07Z: handoff — plan written (48 slices in 6 milestones, SDDM entry removed at M1/PF9; ≈72–120 agent-hours); ./tools/validate-docs exit 0 (424 documents); all 13 relative wiki links resolve. No code, ebuild or profile changed; no ADR used.
- 2026-09-29T02:38:07Z: revision — owner answered Q1 (no early hiding) and Q2 (no Plasma fallback; co-installable KWin fork). Added M1 fork identity (F1–F8, 11 slices) from the 500-file stock manifest, vendored kdecoration, re-sequenced to 60 slices; handoff of the revised plan.
