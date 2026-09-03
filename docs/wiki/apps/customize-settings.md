# Settings Customize route

The installed `qindaqt-settings --page customize` route is QindaQt's direct
layout editor. It renders the selected schema-v1 profile as panels and applet
chips on a scaled 1920×1080 representative output. An audited manifest palette,
keyboard outline, and contextual property panes surround the canvas in both
wide and compact layouts.

The route is a presentation and composition boundary. It depends only on the
public `shell_customization_editor`, `shell_customization`, `profiles`, applet
manifest, and Settings1 client APIs. It never imports shell surfaces,
LayerShellQt, compositor code, a private service implementation, or platform
mutation. The editing repository remains the sole placement and manifest
acceptance authority.

## Direct editing contract

A palette drag or existing-chip drag is one `EditorSession` gesture. Arming
the payload and entering drag mode opens a provisional preview; each target
update converges the repository to that target. Release commits the preview as
exactly one durable Undo step. Escape, leaving a drag without an accepted drop,
or explicit cancellation restores the exact pre-gesture snapshot. Rejected
targets keep their reason visible and cannot partially mutate the draft.

The canvas uses repository-projected panel rectangles, not a second geometry
solver. Drop targets are the three profile zones on each panel. The property
panes issue complete panel configuration or move intents for edge, alignment,
thickness, length, and visibility. They deliberately omit always-hidden mode
until the separate reveal-affordance work exists; creating an unrecoverable
panel from this route would violate fail-closed interaction.

Manifest `settingsSchema` fields and current values are exposed in the selected
applet pane. They are read-only in this slice because the accepted public
editor intent vocabulary has no arbitrary applet-settings intent: the only
`UpdateAppletSettings` sequence it owns is the internal zone update used by a
move. The route does not bypass that boundary by executing engine commands
directly. Duplicate and Remove remain available through public editor intents.

## Keyboard and accessibility

All palette entries, panels, zones, and applets are represented in the focusable
outline even when the scaled canvas is too small for useful pointer targets.
They expose list/list-item or radio-button roles, contextual names and counts,
selected state, and visible QindaQt Controls focus treatment. Palette activation
inserts into the first panel's start zone through the same gesture path as a
pointer drag.

For a selected applet, Space begins or commits keyboard move mode. Ctrl+Left or
Ctrl+Right steps within a panel, Alt+Left or Alt+Right changes zone, and
Ctrl+Shift+Left or Ctrl+Shift+Right changes panel. Delete removes the selection,
Ctrl+D duplicates it, the platform Undo/Redo sequences traverse history,
Ctrl+Return applies, and Ctrl+Shift+Return discards. Announcements describe
accepted and rejected targets for assistive technology. The route's Close and
navigation-away paths do not discard silently: dirty profile selection is
rejected, while Close or selecting another Settings route opens a modal discard
confirmation. Cancelling that prompt returns navigation to Customize.

## Settings1, persistence, and failure truth

`panels.layoutProfile` is the Settings1 selection key. A confirmed snapshot
creates the route baseline and a fresh repository/editor host. Selecting a
different catalog profile is a draft until Apply. Apply first atomically writes
edited profile content through the public profiles store adapter and only then
commits a changed selection through the route-owned Settings1 client. Conflict,
uncertain result, owner loss, and failed profile storage retain dirty truth and
surface an explicit diagnostic; none is reported as success.

Undo/Redo cleanliness compares the full canonical profile with the applied
baseline. Discard rebuilds from the last confirmed catalog profile. Only one
editor coordinator lease exists: losing it makes the route read-only with an
explicit unavailable notice. Retry obtains a new Settings1 snapshot and may
rebuild after the foreign lease is released. Missing catalogs, invalid
manifests, repository failure, and Settings1 transport failure likewise fail
closed.

Built and installed executables discover profiles and applet manifests from
their respective source or relocated `share/qindaqt` catalogs without relying
on a developer-tree working directory. The compiled
`QindaQt.SettingsApp.Customize` module is linked into `qindaqt-settings` and is
also installed with its QML metadata and sources in the
`SettingsAppearanceRuntime` component, alongside the profile and manifest
catalogs.

## Verification

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.settings-customize-' \
  --output-on-failure --no-tests=error
```

The model row proves one-step pointer commit, exact cancellation, deterministic
rejection rollback, pointer/keyboard insertion convergence, Undo/Redo,
atomic profile persistence, Settings1 conflict truth, and foreign-lease
recovery. The page row renders 720×720 compact and 1080×720 wide layouts with a
software `QQuickView`, checks the canvas and contextual controls, validates
accessible names for palette/panel/zone elements, and exercises keyboard
activation. Positive and deliberately poisoned boundary rows reject shell,
LayerShellQt, compositor, private repository, and D-Bus imports. The installed
row reuses the sanitized Settings package harness and proves the relocated
module and catalogs.

These tests use injected transports, temporary stores, and offscreen rendering.
They do not contact a host session bus, compositor, hardware, or input device.

## Stopping point

Apply makes the profile durable and changes the Settings1 selection, but the
production shell reads that choice only on its next start. Provisional live
shell binding, the always-hidden reveal affordance, installed-session behavior,
and the nested rendered matrix are separate later slices. This route therefore
makes no claim that an open shell follows its previews or applied draft live.

See [Customization editor domain](../shell/customization-editor.md),
[Layout profiles](../shell/layout-profiles.md), and
[ADR-0043](../adr/0043-isolate-the-customization-editor-domain.md).
