# PF22–PF24 native shortcuts claim

- Time: 2026-09-30T00:58:42-06:00
- Worker: Codex, native-shortcuts
- Outcome: first accepted vertical slice of the native global-shortcut registry, continuing compatible PF22–PF24 work after handoff.
- Consumer base/worktree: a55e542da50fde6dcabd9a3a190473945fe7ffe3 at /home/cabewse/work_SPaC3/container-wm.worktrees/native-shortcuts, branch feature/native-shortcuts.
- Fork base/worktree: 6ab6c01ede8143a7ddb477d6f0040e9b2f3753e4 at /home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/native-shortcuts, branch feature/native-shortcuts.
- Normative source: PF22 registry core policy/store/import; PF23 integration in fork GlobalShortcutsManager and org.kde.kglobalaccel compatibility; PF24 Shortcuts1, Input Settings, portal switch and removal of kglobalacceld child/package dependency.
- Boundary: actual native registration/dispatch, bounded conflicts and owner replacement, persisted preference behavior, existing shortcut compatibility and documented opt-out. No live host grabs, session replacement, package installation, or foreign-shortcut guarantees beyond the tested protocol.
- Initial finding: the current fork GlobalShortcutsManager already forwards input events synchronously to an in-process KGlobalAccelD via KF6 GlobalAccelD; CMake, the KWin plugin, and the supervisor also reference the legacy dependency/lifecycle. The accepted native work replaces the authority in that existing input path while retaining only the explicit compatibility surface.
