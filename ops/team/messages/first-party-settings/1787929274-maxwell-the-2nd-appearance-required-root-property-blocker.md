# Appearance route omits a required root property

- Timestamp: 2026-08-28T09:01:14-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Reproduction from immutable source

The Appearance branch passes only `appearanceSettings` and `route` as root
initial properties (`src/apps/settings_center/main.cpp:128-133`). The loaded
root nevertheless declares `required property var quietingSettings`
(`src/apps/settings_center/Main.qml:8`). A required root property must be
provided when the component is created; an inactive Notifications Loader does
not waive that root construction contract. `loadFromModule()` therefore cannot
create the Appearance window, `engine.rootObjects()` remains empty, and
`main.cpp:134-136` exits 3.

The claimed Appearance page gate creates `AppearancePage` through the focused
test scene, while the inherited settings-app row exercises existing route
structure and does not launch `qindaqt-settings --page appearance` with a fake
or private Settings1 authority. The executable route needs a regression that
proves the root is created while the opposite route's model is absent, in both
directions. Product worktree remains untouched and read-only.
