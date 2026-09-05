# Preferences: theme-map propagation coordination + blocker repair status

2026-09-05T17:25Z — audit-fix-preferences.

**Ownership extension notice.** Per the manager's blocker-3 requirement (live
raw theme maps), this change touches two files outside my original list:
`src/shell/runtime/runtimepanelwindowfactory.{h,cpp}` and
`src/shell/runtime/notificationwindowcontroller.{h,cpp}`. The edits are
additive: each class gains a `setTheme(QVariantMap)` that replaces the cached
map used for future windows and pushes the confirmed map onto existing live
windows through the shared header-only helper
`src/shell/runtime/thememappropagation.h`. No constructor signatures, QML, or
applet access interfaces changed. Customize worker (`shell/qml`,
desktop-controls hooks): no overlap — the QML `theme` properties stay plain
`var` properties exactly as they are; this path only writes to them from C++.

**Three blockers from the independent review are repaired, not deferred:**

1. Saved-but-deleted profile falls back to `qindaqt` with a diagnostic;
   explicit unknown `--profile` still exits 2. Covered by the new
   `qindaqt.shell-runtime-startup-launch` test driving the real shell binary
   against a private Settings1 service (recovered exit 3, explicit exit 2,
   absent service exit 3).
2. `fonts.family` joins the scoped preference set and overlays the derived
   token `type.fontFamily` at startup and live, mono family untouched.
3. Raw panel/notification theme maps update live on confirmed theme/font
   changes for existing and future surfaces, alongside QST token publication.

Focused ctest: 8/8 then 4/4, both exit 0. mkdocs --strict and validate-docs
exit 0. ADR-0074 updated; it no longer defers the map gap and documents that
wallpaper/uiScale/rendering preferences are not consumed by the shell.
