# Trachette Jackson — independent shell/application exact-candidate review

- Persona: Trachette Jackson, independent shell/application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `59353bf431b3a9d19f20e9db23617cb839fd1dda`
- Tree SHA: `e69799515d99b809daa7e8be7cb1bfe661e6ac35`
- Parent SHA: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`
- Base SHA: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f` (`f84d3ae`)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex`

## Findings ledger

### P0

None.

### P1

#### P1-01 — A rejected close permanently withdraws the live window's exported menu

`src/app_shell/menu_export/src/application_menu_export.cpp:398-405` calls `stop()` from an event filter as soon as a `QEvent::Close` is delivered. Event filters run before the window handles the close. The AppShell contract at `src/app_shell/qml/ApplicationShell.qml:25-35` deliberately rejects the first close while it requests in-window consent, so the window stays alive but its registrar association and exported D-Bus object are removed. Nothing restarts the composition after that rejected close. The candidate test at `tests/app_shell/tst_application_menu_export.cpp:221-225` sends an accepted close to a bare `QWindow` and therefore cannot detect this failure.

Minimal reproduction (scratch source is outside the worktree at `<ROOT>/repros/ignored-close/repro.cpp`):

```text
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build -G Ninja
# exit 0
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build --parallel 1
# exit 0
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build/ignored_close_repro
# exit 1
version4Methods GetProperty=0 EventGroup=0 AboutToShowGroup=0
closeAccepted=0 statusAfterRejectedClose=0 published=0
```

The reproducer uses a `QWindow` whose close handler calls `ignore()`. Expected: `closeAccepted=0` and `published=1`. Observed: the close is rejected, but status is `Disabled` (`0`) and `published=0`.

#### P1-02 — AppShell composition mints a second lineage authority

The common lane contract requires the AppShell export composition never to mint lineage. Instead, `src/app_shell/menu_export/src/application_menu_export.cpp:107-130` defines `LocalExportLineage`, creates a UUID epoch, and advances its own revision; `:142-157` also creates an owner UUID and advances that lineage for catalog changes. This is a second owner/epoch/revision issuer alongside the accepted shell selector lineage authority described by ADR-0033. Even if the shell decoder later replaces these values, the composition violates the required single-lineage boundary rather than transporting application menu content without inventing trust metadata.

Minimal source-contract reproduction:

```text
if rg -n 'class LocalExportLineage|QUuid::createUuid\(\)|lineage\.advance\(\)' src/app_shell/menu_export/src/application_menu_export.cpp; then exit 1; fi
# exit 1
107:class LocalExportLineage final : public Exporter::ExportLineageSource {
110:      : m_ownerWindowId(ownerWindowId), m_epoch(QUuid::createUuid()) {}
142:        ownerWindowId(QUuid::createUuid()),
157:    lineage.advance();
```

Expected: no lineage issuer in the AppShell composition. Observed: locally minted owner, epoch, and revision.

#### P1-03 — The candidate adds a second, incomplete dbusmenu v4 server outside the accepted transport boundary

`src/app_shell/menu_export/src/dbusmenu_export_object_p.h:19-52` privately declares a new `com.canonical.dbusmenu` server in AppShell rather than using a server boundary owned by the accepted global-menu transport module. `src/app_shell/menu_export/src/dbusmenu_export_object.cpp:75` advertises protocol version 4, but the object exposes only `GetLayout`, `GetGroupProperties`, `AboutToShow`, and `Event`. It omits v4 interface methods `GetProperty`, `EventGroup`, and `AboutToShowGroup`; `GetLayout` and `GetGroupProperties` at `:102-120` also discard their depth/property filters. For a local authoritative comparison, `/usr/include/qt6/QtGui/6.11.1/QtGui/private/qdbusmenuadaptor_p.h:38-145` declares the complete Qt dbusmenu v4 adaptor surface.

The same private-bus introspection reproduction used for P1-01 exits 1 and reports:

```text
version4Methods GetProperty=0 EventGroup=0 AboutToShowGroup=0
```

Expected: the advertised standard v4 interface contains all three methods and the implementation comes through one accepted exporter/dbusmenu seam. Observed: all three methods are absent from D-Bus introspection, and the server is a new AppShell-private protocol implementation. The focused QindaQt decoder remains green because it only calls the candidate's supported subset; that does not prove interoperability with a standard v4 consumer.

### P2

#### P2-01 — The real File Manager shell row omits the required PID and window-ID mismatch variants

`tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp:104-112` declares one test slot. Its only identity publication at `:172-175` supplies the real child PID and matching window ID `77`; there is no negative variant for a mismatched PID or registrar window ID. Older generic ownership tests cover PID checks, but the new real-process row is the only proof that this exact File Manager export path is fenced end to end, and it does not exercise either required hostile identity variant.

Minimal source-test reproduction:

```text
rg -n 'private Q_SLOTS|shellConsumesRealFileManager|publishIdentity\(' tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp
if rg -n 'mismatch|wrong-(pid|window)|rejects.*(pid|window)' tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp; then exit 0; else echo 'missing PID/window mismatch cases'; exit 1; fi
# exit 1
37:  bool publishIdentity(qint64 processId, quint32 registrarWindowId) {
107:private Q_SLOTS:
108:  void shellConsumesRealFileManagerAndActivatesExactlyOnce();
112:    shellConsumesRealFileManagerAndActivatesExactlyOnce() {
172:  QVERIFY(transport.publishIdentity(
missing PID/window mismatch cases
```

Expected: real File Manager success plus PID-mismatch and window-ID-mismatch rejection cases. Observed: one success-only slot.

### P3

None.

## Review-question evidence

1. **Lineage discipline:** fails P1-01 and P1-02. The exact private-bus tests do show truthful registrar owner loss/reacquisition and exactly-once activation for their covered accepted path, but rejected close is broken and the composition mints owner/epoch/revision locally.
2. **Single seam / bus injection:** fails P1-03. The public composition does take an injected `QDBusConnection`, and `rg 'QDBusConnection::sessionBus\(' src/app_shell/menu_export` found no ambient session-bus lookup. The private protocol server is nevertheless a second, incomplete implementation.
3. **File Manager composition and local menu:** the production `main.cpp` adds one guarded composition, retains the local AppShell menu bar, and the installed AppShell/File Manager rows pass in both configurations. These facts do not cure P1-01.
4. **Real File Manager integration:** the private-bus row starts the real production binary, binds the matching child PID/window identity, observes one activation, and remains alive. It lacks the required mismatch variants (P2-01).
5. **Scope/static/docs:** `git diff f84d3ae..59353bf -- src/shell` is empty; no Text Editor, Terminal, GTK bridge, nested compositor, host-bus, hardware, or network surface was added or exercised. The candidate ADR is indexed and the docs gates pass. No JSON changed. `application_menu_export.cpp` is 408 physical lines and remains below the source-shape decomposition threshold.

## Commands and results

### Immutable tree checks

```text
pwd
# /home/cabewse/work_SPaC3/container-wm-workers/app-menu-export-codex-review
git rev-parse HEAD
# 59353bf431b3a9d19f20e9db23617cb839fd1dda
git rev-parse HEAD^{tree}
# e69799515d99b809daa7e8be7cb1bfe661e6ac35
git rev-parse HEAD^
# f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f
git rev-parse f84d3ae
# f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f
git status --porcelain
# exit 0, no output (before review)
```

### Configure and focused builds

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
```

Both configurations emitted the documented Qt `GuiPrivate` version-coupling warning and pre-existing dependency-root RPATH warnings; neither configuration failed.

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0, 825/825 steps
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0, 821/821 steps
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release --parallel 3 --target qindaqt_global_menu_qmlplugin
# exit 0, 4/4 steps
```

The explicit QML plugin target is necessary for the installed-package and QML rows. Before adding it, an exploratory Debug selector run exited 8 with 37/44 passing and seven failures caused by missing `libqindaqt_global_menu_qmlplugin.so`; after building the target, the exact gates below are green. This was treated as review-command target completion, not a product finding.

### Focused and adjacent tests

```text
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 44/44 passed; 0 failed; 11.17 s
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 44/44 passed; 0 failed; 10.17 s
```

No `tests/session` nested-compositor rows, host system/session bus, hardware, uinput, or network were used. Every D-Bus exercise used a private `dbus-run-session` created by the test or scratch reproducer; the ambient bus was removed and the system bus was fenced to `/nonexistent`.

### Static and documentation gates

```text
./tools/validate-docs
# exit 0; Validated 131 Markdown documents and mkdocs.yml navigation.
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/site
# exit 0; documentation built in 1.75 s
./tools/check-source-shape
# exit 0; checked 2128 files, skipped 0; four warnings, all pre-existing files outside this candidate
git diff --check f84d3ae..59353bf
# exit 0, no output
git diff --name-only f84d3ae..59353bf -- '*.json'
# exit 0, no output; no changed JSON to validate
```

## Verdict

REJECT. The candidate has three P1 contract/correctness failures and one P2 missing negative control. In particular, a normal denied-close consent path withdraws the menu from a still-running File Manager, and the new server does not provide the complete dbusmenu v4 interface it advertises.

VERDICT REJECT P0/P1/P2/P3=0/3/1/0
