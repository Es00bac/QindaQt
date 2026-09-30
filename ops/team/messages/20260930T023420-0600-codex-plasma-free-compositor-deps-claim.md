# PF11 claim — codex-plasma-free-activities

- Time: 2026-09-29T20:34:20-06:00
- Fork source of truth: base 79fa351f6e823d302996453e12356a674e4b64ca, branch feature/plasma-free-compositor-deps, worktree /home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/plasma-free-compositor-deps.
- Consumer source of truth: base a33d33c430c71ac63062d1def44c61e7a3c00ce5, same branch, worktree /home/cabewse/work_SPaC3/container-wm.worktrees/plasma-free-compositor-deps.
- Ownership: fork build options/find_package, retired classic decoration/backend dependencies and Plasma QML effect removal/QtQuick ports. Consumer ownership limited to corresponding QindaQt decoration and switcher glue/QML. Preserve QindaQt decoration and switcher behavior plus PF10 activity-neutral semantics. ADR-0311 reserved.
- Read the complete docs/plans/2026-09-28-plasma-free-qindaqt.md, consumer compositor-session and hybrid-chrome wiki, fork qindaqt/README.md and fork AGENTS. PF11 matches the assigned scope; owner decision 6 says the Plasma-dependent KWin tiles editor is unused and should be removed with those effects. No expansion into packaging, source pins, Lock1/power, resident appearance/keyring, or portals.
- Manager owns shared-source pin/integration; the native-lock worker owns power/Lock1; keyring owns portal/resident appearance. Only qinda is used. No install/deployment, laptop build, or live desktop session. Compiler actions must use the shared qinda flock and actual -j24 -l24.
