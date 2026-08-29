# Settings1 repaired-candidate final release recheck — ACCEPT

- Reviewer: `codex-settings-final-release-reviewer`
- Timestamp: `2026-08-27T13:25:11-06:00`
- Exact candidate: `3de6bfae911594804e00a913f2feef5f1b36e16e`
- Parent repair boundary: `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- Original cumulative base: `496e5135ee4f40359f8b871eec130f0b8b02a241`
- Verdict: **ACCEPT**. No P1, P2, or P3 finding remains.

## Source and contract decision

I reviewed the exact one-commit repair and the complete five-commit Settings1 candidate, not Ada's prose. At `src/services/settings_service/src/main.cpp:34-46,61`, production captures one `QDBusConnection`, binds that exact connection's standard `org.freedesktop.DBus.Local.Disconnected` signal to `QCoreApplication::quit()` before schema/repository/service startup, fails startup if observation cannot be installed, and passes the same connection to `ResidentSettingsService`. It never reconnects the stale repository. This preserves one daemon/unique-owner/epoch lineage per process; replacement activation constructs a new process, repository, owner, and epoch. The new process test's exact-executable cleanup and two-daemon flow are consistent with that contract. The settings-service wiki, ADR-0012, and testing-harness changes accurately describe the implemented lifetime policy.

The cumulative audit also reaccepted the previously reviewed boundaries: bounded recursive D-Bus decode before allocation; canonical JSON including null, wide signed/unsigned values, doubles, and Unicode across save/restart; exact unique-owner/epoch/revision fencing; uncertain-write resync with no replay; service activation rollback; DND Loading/Ready/Saving/Conflict/Unavailable truth; lock-privacy precedence; accessibility and route behavior; dependency direction; source limits; and reference/ADR accuracy.

## Fresh reviewer evidence

- Exact checkout: detached `git rev-parse HEAD` returned `3de6bfae911594804e00a913f2feef5f1b36e16e`; worktree was clean before and after. Both `git diff --check 2a1e262..3de6bfa` and `git diff --check 496e513..3de6bfa` exited 0.
- Fresh Debug and Release configurations with shared libraries, testing, production shell, strict warnings, KWin plugin disabled, and host uinput tests disabled built `864/864` each, status 0. Fresh production Release and fresh installed Release configurations built `429/429` each, status 0.
- Inspectable CTest discovery with `ctest -N -R '^qindaqt[.]settings-'` selected **16** tests in each Debug and Release tree. The matching focused runs passed **16/16** in each tree, status 0.
- Inspectable full discovery with `ctest -N -R '^qindaqt[.]'` selected **82** tests in each tree. The matching full runs passed **82/82 Debug** and **82/82 Release**, status 0. These registries include the settings persistence/protocol/service/client/DND/transport/UI/route/bridge regressions and the broader QindaQt suite.
- `qindaqt.settings-service-process-lifecycle --repeat until-fail:20` passed **20 consecutive iterations in Debug** and **20 in Release**. Direct verbose runs passed **3/3** phases in both configurations.
- Direct verbose boundary runs in both Debug and Release passed: repository **6/6**, real private-D-Bus service lifecycle **4/4**, and public commit-reply validator **7/7**. The validator coverage rejects contradictory/fabricated UnknownKey maps as uncertain, schedules resync, and never replays the uncertain write.
- `all_qmllint` exited 0 in Debug, Release, production, and installed build trees. It emitted only established unrelated shell-preview warnings; this repair changes no QML.
- `tools/check-source-shape` exited 0: **769 source files**, zero skips/violations. `tools/validate-docs` exited 0: **42 Markdown documents** plus navigation. `uvx --offline --from mkdocs mkdocs build --strict` exited 0.
- Fresh `cmake --install` exited 0 and produced **159 staged files**. The installed descriptor `share/dbus-1/services/org.qindaqt.Settings1.service` names the exact staged executable, which has no missing runtime libraries. The committed lifecycle test repointed to the installed descriptor/executable/schema passed **3/3**.

## Independent installed private-D-Bus proof

The independent staged-only probe used two successive real private `dbus-daemon` instances and never attached to the user session bus:

- First activation: PID `3466655`, owner `:1.1`, epoch `7b8737b2-bc36-4f68-8999-be00f522e55f`. Permanent daemon loss caused the exact installed PID to disappear in **56 ms** without explicitly stopping it.
- Replacement activation: distinct PID `3466836`, owner `:1.2`, epoch `b0fa18df-c43e-41ae-ace5-e95bb4de2def`. Second permanent daemon loss caused that PID to disappear in **18 ms**.
- On each daemon, unknown set, unknown remove, mixed known/unknown, and stale-base/unknown returned exact `UnknownKey` ordinal **7**, revisions `0 -> 0`, and empty `changedKeys`, `values`, and `sourceLayers`. Wrong-epoch/unknown returned ordinal **6**, proving epoch precedence. A malformed unknown operation returned ordinal **8**, proving envelope validation precedence. The stale-base case proves UnknownKey precedes revision conflict; repository coverage also proves it precedes revision exhaustion.
- Both post-call snapshots remained revision 0 with `services.doNotDisturb=false`; each signal monitor counted **0** `SettingsChanged` emissions; the user file remained absent. Replacement PID/owner/epoch were all distinct. After the second loss, exact installed orphan count was **0**.
- Failure cleanup was exercised deliberately by pointing the new harness at the preserved rejected `2a1e262` staged binary. It failed at the expected daemon-loss assertion after observing the old resident survive, then the exact-executable RAII cleanup removed that PID. Post-failure old-service count was **0**.

Final anchored `/proc/*/exe` checks found **0** processes for both the repaired staged service and the preserved rejected staged service. No private bus, monitor, placeholder, or service fixture was left resident. No live desktop, user session bus, compositor, KGlobalAccel, input injection, cursor, or lock action was used.

## Bounded caveats and manager action

The only bounded caveats are the established unrelated QML lint warnings and intentionally disabled out-of-scope KWin/host-uinput lanes; neither intersects this settings-service lifetime repair. All required candidate gates passed with no release-blocking or advisory finding.

Manager action requested: integrate exact commit `3de6bfae911594804e00a913f2feef5f1b36e16e`, rerun the affected integration-tree gates, update task/handoff state in that integration, and preserve the accepted commit before retiring its worktree.
