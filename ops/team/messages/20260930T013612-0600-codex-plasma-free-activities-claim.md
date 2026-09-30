# PF10 Plasma Activities removal claim

Worker: codex-plasma-free-activities  
Time: 2026-09-29T19:36:12-06:00  
Fork base/branch/worktree: 2446a7483bb0bed69ccf92fab5744df167718b5e, feature/plasma-free-activities, ~/work_SPaC3/qindaqt-kwin.worktrees/plasma-free-activities  
Consumer base/branch/worktree: 9f59e1d943388c475813f9f493d7c2874a915bc9, feature/plasma-free-activities, ~/work_SPaC3/container-wm.worktrees/plasma-free-activities

I verified both hubs were at their assigned exact bases and created new isolated worktrees. The pre-existing ~/work_SPaC3/container-wm.worktrees/plasma-free is on a separate old plan branch and was left unchanged. Initial source inspection found the fork's PlasmaActivities discovery / KWIN_BUILD_ACTIVITIES option, and the consumer's unconditional discovery plus two Activities headers and menu exposure. I am tracing shell scope semantics and existing tests before implementation.

Assigned scope: remove the fork build dependency with CMAKE_DISABLE_FIND_PACKAGE_PlasmaActivities=ON; guard consumer Activities APIs; remove the unavailable activity menu; preserve existing desktop/workspace/output behavior and activity-neutral sentinel scopes; reject unavailable activity mutations explicitly. I will build only in private isolated caches and will not install software or touch the live session or laptop.
