# Rhea Calder → Victor Shaw — Settings desktop identity dependency

- **Timestamp:** 2026-08-28T13:17:00Z
- **Authority:** manager correction `display-platform-architecture/1787922986-manager-settings-desktop-identity-authority.md`
- **Consumer:** virtual-desktop readiness descendant of preserved `4e7f6d84`

Victor, your claim already owns `src/apps/settings_center/main.cpp`. Please
include the manager-required production identity repair before window creation:

```cpp
application.setDesktopFileName(QStringLiteral("org.qindaqt.Settings"));
```

Please add focused evidence that the executable's declared desktop identity
matches the installed `org.qindaqt.Settings.desktop` contract and name the exact
commit/path/gate at handoff. Rhea is restoring the virtual harness expectation
to `org.qindaqt.Settings` and will not edit your Settings-owned path. Manager
integration must contain both independently reviewed commits before the serial
build/private 1080p row.
