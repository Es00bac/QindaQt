# Remaining retired package parents: bounded metadata result

One nice19/idleIO qinda pass exited0 in1.619s, reading459,473 metadata bytes across1567 installed CPVs (1541 non-retired parents,26 retired skipped). Both actual RDEPEND/PDEPEND fields were reduced under installed USE; zero parse failures. Matching used installed Portage vartree metadata, preserving slot/USE constraints and ignoring blocker atoms. No source/build/payload/GUI/host action.

| Installed non-retired parent | Repository | Exact active atom | Finding | Existing minimal resolution |
| --- | --- | --- | --- | --- |
| gui-apps/xwaylandvideobridge-0.5.0 | gentoo | RDEPEND kde-plasma/kpipewire:6 | Mandatory direct dependency matches kpipewire-6.6.6.1. IUSE only debug. | No package-use switch: installed recipe unconditionally requires library in DEPEND and RDEPEND. Preserve app/library unless separately authorized compatible replacement. |
| app-misc/claude-desktop-bin-1.49585.0 | sloom-private | RDEPEND OR: KDE portal / GTK portal / GNOME portal | KDE6.6.6 and GTK1.15.3 both installed; GNOME portal absent. OR declaration is not proof KDE uniquely required or solver-selected. | Installed IUSE only cowork. Reviewed qindaqt Claude-r1 native conditional remains queued behind actual Desktop/provider transition. Existing GTK also satisfies current OR; no solver rerun made here. |

Old gui-wm/qindaqt-desktop is excluded from independent blockers and preserved separately:9 active direct rows retain KWin, KDecoration, KScreenLocker, Polkit agent, Activities, PowerDevil, Knighttime, Spectacle and KDE portal. Their exact slots, versions, dependency row, repository and USE are recorded in JSON.

Twenty-four of26 remaining retired CPVs have no independent direct non-retired parent in this bounded snapshot. That is NOT authorization or proof they can be removed: retired-parent chains, world protection, provider transition and Portage selection remain manager-owned. Full closure/resolver/runtime acceptance is not claimed.

Summary binding: `/home/cabewse/work_SPaC3/.qa/pf-desktop-portage-final-20261002/root-interim-retirement-summary.json`, SHA `f509d51775bae249845d0d9c13ba263162345c23100fe6247ec61804eac10b8d`. Exact two parent CPVs/repos/USE/IUSE/installed-recipe hashes and complete runtime dependency rows are in `ops/team/messages/pf-power-delivery-20261002/20261002T225126Z-remaining-retired-independent-parents-review.json`. Source3e05 isolated WT remains preserved. Worker is available/idle, no compiler/runtime lease; a further task needs a bounded grant.
