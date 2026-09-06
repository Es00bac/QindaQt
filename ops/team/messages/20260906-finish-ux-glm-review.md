# Settings usability audit — finish-ux-glm (2026-09-06)

Bounded, read-only audit of all Settings pages (`src/apps/settings/**`, `src/apps/settings_center/**`) plus the global-menu applet, against the user's report: "Settings looks AI/web-designed, non-human-friendly, pages mix unrelated settings, global menu flashes, many services unavailable." No product files were modified. Owners of the services, navigation, appearance, and customize repairs can map these findings onto their lanes; nothing here edits their scope.

Method: every QML page and the user-facing strings of every settings model were read; line references below were re-verified against the current tree. Sub-findings are examples, not exhaustive lists.

---

## Top 8 problems, prioritized

### 1. Protocol, service, and model vocabulary leaks into user-visible text on every page — the single strongest "not designed by a human" signal

Evidence (all user-visible):
- Service names: `NetworkDeviceSection.qml:19` "Authoritative device and active-connection state from Network1"; `AudioStreamSection.qml:60` "…levels from Audio1"; `BluetoothAdapterSection.qml:19` "…from Bluetooth1"; `clipboard_settings_service.cpp:111` "Settings1 is unavailable…"; `CustomizePage.qml:167` "The profile repository, editor lease, or Settings1 transport is unavailable."
- D-Bus/OS jargon: `network_settings_model.cpp:223` "No secret agent running — secured networks cannot prompt."; `BluetoothAdapterSection.qml:83-87` "Acquire one bounded discovery lease for %1"; `BluetoothPage.qml:178` "Close Settings and release any discovery lease"; `power_settings_projection.cpp:79` "Read-only in Power1 version 1"; `PowerBrightnessSection.qml:42` "…Power1 version 1 has no display-brightness mutation".
- Raw reason codes passed verbatim to labels: `clipboard_settings_service.cpp:232` `m_clearError = result.reasonCode.left(512);` (kebab-case codes like `stale-handle` shown to users); same pattern in `bluetooth_settings_model.cpp:517` "not admitted (%1)", `audio_settings_model.cpp:361`, `color_settings_model.cpp:342`, `display_settings_model.cpp:373-377` "Display transaction outcome uncertain: %1".
- Internal keys and counters: `appearance_settings_model_results.cpp:63-70` builds "Save results: appearance.theme — Applied; fonts.family — Applied" from raw setting keys; `AudioPage.qml:253-256`, `PowerPage.qml:186-189` "Epoch %1, revision %2"; `ClipboardPage.qml:202-207` "%1 of %2 metadata-only slots currently used · epoch %3 · generation %4 · revision %5"; `ColorPage.qml:191-199` "Display revision %1, settings revision %2".
- Stable IDs as names: `ColorOutputSection.qml:103` shows `modelData.id` for disconnected displays; `color_settings_projection.cpp:76` "Profile %1 (not in the discovered catalog)".
- Reliability jargon in normal-state text: "authoritative" (`audio_settings_projection.cpp:135` "Authoritative audio state is shown."), "admitted" (`power_settings_model.cpp:114`, `color_settings_model.cpp:125`), "fail closed" (`CustomizeAppletProperties.qml:47`), "not replayed" (`bluetooth_settings_model.cpp:465`, `network_settings_actions.cpp:138-139`).

Desktop-native fix: introduce one message-mapping layer per model that converts reason codes and internal keys to human sentences (a mapping function already exists in `color_settings_model.cpp:462-487` — extend that pattern); delete epoch/generation/revision from all footers (debug logging instead); ban service/protocol names from `qsTr`/`tr` strings; reserve "authoritative/lease/admitted/lineage/transaction" for code comments. This is mostly string work and is the cheapest high-impact fix in the audit.

### 2. Customize shows raw wire-enum tokens as the actual button labels users click

- `CustomizePanelVisibilityProperties.qml:26` — `model: ["never", "intelligent", "dodge-active", "dodge-all", "maximized"]` rendered verbatim as `text: modelData`; the user picks "dodge-active".
- `CustomizePanelPositionProperties.qml:27-31` — panel edge choices are literally "top/right/bottom/left"; `:52-55` — alignment is "start/center/end/fill" (GTK/CSS concepts).
- Related: `CustomizePanelVisibilityProperties.qml:20` — "Always-hidden is unavailable until reveal controls land" leaks a roadmap milestone.

Desktop-native fix: token→label pairs as Appearance already does (`AppearanceThemeSection.qml:101-103` uses `{ token, label }`): "Always visible / Auto-hide / Dodge active window / Dodge all windows / Hide when maximized", "Top/Right/Bottom/Left", "Left/Center/Right/Full width". (Customize repair owner can fold this in.)

### 3. Global menu flashes on every focus change because the applet collapses to zero while the coordinator republishes

Mechanism (code-backed):
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp:199` — on binding a new provider, the coordinator calls `m_applet.publishUnavailable()` and only then starts the async `DbusMenuClient`; the new tree arrives later via `treeChanged`. So available→unavailable→available on every window switch.
- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml:143-144` — `implicitWidth/implicitHeight` are `available ? … : 0`, so during the gap the applet physically disappears and the panel row reflows, then pops back — the user's "globalmenu flashes".
- `GlobalMenuApplet.qml:204` — the Repeater's model is a plain JS array (`visibleEntries`), so every tree republish rebuilds all menu delegates.
- Compounding it: Settings itself exports no application menu (no AppShell/menu export anywhere under `src/apps/settings_center`), so focusing Settings collapses the applet entirely; alt-tabbing between Settings and any menu-exporting app makes the panel jump.

Desktop-native fix: keep the menubar slot reserved (fixed height; show the focused app's name — or the previous menu dimmed — while the new tree loads, like macOS/KDE appmenu); double-buffer: publish the new tree before withdrawing the old one, and only collapse after a grace timeout if no provider appears; keep delegates keyed by id instead of rebuilding from a fresh array.

### 4. Standard desktop pickers replaced by free-text fields that silently eat input

- Wallpaper: `AppearanceDesktopSection.qml:92-111` — a `TextField` where the user must type a filesystem path; a bundled `qindaqt:` wallpaper shows as an empty field (`:103-104`) with no explanation. Color already demonstrates the native pattern — `ColorImportSection.qml:52-55` uses `FileDialog`.
- Font family: `AppearanceFontSection.qml:42-53` — type "Noto Sans" exactly; no font list, no preview. QFontDatabase families → ComboBox is the native control.
- Monitor arrangement: `DisplayArrangementSection.qml:51-95` — multi-monitor layout by typing raw X/Y pixel numbers; and `DisplayCoordinateField.qml:86` (`onEditingFinished: resynchronize()`) silently discards the edit on blur/focus loss, so a typed coordinate can vanish before Apply.
- Bluetooth device "icons": `bluetooth_settings_projection.cpp:66-84` renders two-letter text abbreviations ("HS", "PC") while `deviceIconName()` already produces freedesktop icon names the QML never consumes.

Desktop-native fix: FileDialog + preview for wallpaper; font ComboBox with preview; a draggable monitor canvas (numeric fields only as an accessible fallback, committing on blur); themed icons from the names already produced.

### 5. Pages mix unrelated settings, and the same setting appears twice with different names

- Appearance holds six groups: theme, color scheme, font family/size, antialiasing/hinting/subpixel order, wallpaper + mode, and **display scale** (`AppearanceDesktopSection.qml:136-176`, titled "Display scale intent", description admitting "it is not applied by this window") — while the Display page has its own "Scale & Layout" (`DisplayScaleSection.qml`). Two scale controls with near-identical labels ("Logical UI scale" vs "UI Scale"), different value idioms (`%3x` on `DisplayOutputCard.qml:102` vs "150%" buttons).
- Power mixes battery supplies, power profiles, read-only brightness (permanently disabled slider at `PowerBrightnessSection.qml:65-73`, raw sysfs text "Raw 523 of 1509" at `:80-83` via `power_settings_projection.cpp:197-199`), keyboard backlight, **and session Lock/Log out/Suspend/Restart/Shut down buttons** (`PowerPage.qml:152-155`) — logout/shutdown are session controls, not power-hardware settings; the page title "Power and brightness" (`PowerPage.qml:63`) confirms the grab-bag.
- Display vs Color split: two top-level pages for one physical concern (monitors vs ICC per display), with Color's permanent explainer admitting the page doesn't apply anything (`ColorPage.qml:104-110` "Assignments are stored intents … does not apply profiles to the compositor; color application is a separate authority").
- Clipboard is categorized "Personalization" (`settings_route_registry.cpp:206`) though it's a utility behavior; Notifications is a full page holding a single switch (Do Not Disturb, `NotificationsPage.qml:30-48`) while its sidebar description promises "Do Not Disturb, alerts, and quieting" (`settings_route_registry.cpp:81`).
- Customize interleaves global layout-preset switching with per-panel pixel editing on one screen, and blocks preset switching with an error while dirty (`CustomizePage.qml:109-133`, `customize_settings_actions.cpp:94`).

Desktop-native fix: one concern per page — move display scale to Display (delete the Appearance copy), brightness to Display, session buttons out of Settings (session menu), merge Color into Display as a section or make assignments actually apply, and either fill Notifications with real notification settings (per-app, banners, sounds) or fold DND into a general page.

### 6. "Service unavailable" pages masquerade as "no hardware", and recovery differs five ways

When a backend is down, the pages keep rendering live-looking empty lists below the error card:
- `AudioDeviceSection.qml:200-201` "No output devices are currently reported."; `BluetoothDeviceSection.qml:159` "No Bluetooth devices are currently reported."; `NetworkDeviceSection.qml:79` "No network devices are currently reported."; `PowerPage.qml:87` similarly. A user reading "no output devices" concludes their hardware is gone.
- Retry is different on every page: Network = card "Retry" + footer "Reload" (`NetworkPage.qml:72,200`); Audio = card action + footer Retry (`AudioPage.qml:93,243`); Power = footer only with jargon label "Reconnect to Power1 and reload authoritative state" (`PowerPage.qml:180`); Bluetooth = **no retry at all** (`BluetoothPage.qml:61-71`); Clipboard = "Retry Settings" (`ClipboardPage.qml:193`); Display shows **two simultaneous retry controls with different labels** ("Retry Connection" card at `DisplayPage.qml:114-124` plus footer "Retry" at `:235-242`); Appearance leaves a fully-rendered, dead, disabled form with only a muted label (`AppearancePage.qml:119-127`); Customize's degraded state (`CustomizePage.qml:167`) has no retry either.

Desktop-native fix: one shared degraded-state component used by every route — hide/disable the live-looking inventory with one sentence ("Network service isn't running, so these settings can't be shown."), exactly one "Try again" action, consistent wording. This directly addresses the "many services unavailable" complaint being confusing rather than actionable. (Services owners fix availability; this is the presentation contract they need.)

### 7. Commit model and action bars are web-form idioms: per-page Apply/Revert/Close, Close quits the whole app, and shortcuts skip Ctrl+5

- Appearance/Display use a draft+Apply web flow for settings desktops apply instantly (theme, wallpaper, fonts — `AppearancePage.qml:213-240`), and the Apply button **vanishes** while saving instead of showing progress (`:235` `visible: !saving`; the shared Button supports `busy`, `src/controls/qml/Button.qml:34`).
- Clipboard wraps a single boolean switch in Apply/"Undo draft"/"Apply my choice" buttons (`ClipboardPage.qml:168-197`) — no sibling page works this way.
- Color applies instantly with no Apply — three commit models coexist across ten pages.
- Every page embeds a **Close button that closes the entire application** (`AppearancePage.qml:242-253`, `NotificationsPage.qml:102-112`, and the same `onCloseRequested: root.close()` wiring for all ten routes at `Main.qml:276-353`) — a modal-dialog idiom inside a settings window; the window's own titlebar close already exists.
- Same concept, different verbs: "Revert" (`AppearancePage.qml:228`) vs "Discard" (`CustomizeActionBar.qml:48`).
- Keyboard shortcuts Ctrl+1…Ctrl+0 jump to routes but skip Ctrl+5 — Customize (route #5) has no binding while Ctrl+4=Network and Ctrl+6=Audio (`Main.qml:67-110`); Escape refocuses the sidebar (`Main.qml:167-169`), which no desktop settings app does.
- The window title changes on every route switch ("QindaQt Settings — %1", `Main.qml:46`), redrawing the titlebar each navigation.

Desktop-native fix: instant-apply with in-place busy state and undo for reversible settings; reserve explicit Apply for genuinely risky changes (display mode keeps its countdown banner — that part is already native, `DisplayPreviewBanner.qml:42-44`); delete in-page Close buttons; one shared footer vocabulary; give Customize its Ctrl+5 or drop the numeric scheme for standard list navigation.

### 8. Four different custom idioms for the same "choose one of N" semantic — none with radio keyboard behavior

- `SegmentedChoiceRow.qml:12,26,37` — custom pill row claiming `Accessible.role: RadioButton` with no arrow-key movement between items (Tab-only).
- Flow of checkable full-width Buttons — `DisplayScaleSection.qml:44-75` (seven scale presets, no custom value), `DisplayTransformSection.qml:49-71`, plus the raw-token variants in Customize (finding 2).
- A single genuine `T.ComboBox` — only for resolution (`DisplayModeSection.qml:37-46`).
- Full-width button lists with "Set default" per row — `AudioDeviceSection.qml:143-157`, `ColorOutputSection.qml:49-69`, `PowerProfileSection.qml:49-60`.

Desktop-native fix: one rule for exclusive choice: radio group (with arrow keys) for ≤4 options, ComboBox beyond, applied uniformly; audio default device as a ComboBox labeled "Output device"; power profiles as a radio row. Also standardize heading capitalization (Display mixes "Resolution & Refresh Rate" Title Case with sentence-case siblings, `DisplayModeSection.qml:25`) and switch labels ("Enabled/Disabled" vs "Primary Display/Secondary Display", `DisplayOutputSection.qml:56` vs `DisplayArrangementSection.qml:38`).

---

## Additional cross-cutting notes (lower priority, still general)

**Navigation chrome**
- The sidebar defines an `iconName` per route (`settings_route_registry.cpp:82,98,114,…`) but no icon is ever rendered — buttons are text + a small category caption that disappears when active (`SettingsNavButton.qml:74-83`). Ten icon-less, flat, ungrouped entries is the web-sidebar look; desktop convention: grouped sections with icons, category headers once (General/Personalization/Hardware currently interleave in registry order).
- Compact mode (width < 540, reachable at the 420px minimum, `Main.qml:36,43`) puts all ten route tabs in one fixed 48px row with no wrap, scroll, or clip (`SettingsCompactHeader.qml:14,55-98`) — they overflow and paint over the page. Native fallback is a ComboBox category selector.
- Clicking an unavailable route does nothing with no feedback (`SettingsSidebar.qml:71-75`); the `unavailableReason` is passed but all built-ins are `available = true`, so the centered "This settings page is unavailable." (`Main.qml:355-369`) appears without explanation when a component is missing.
- Alt+Left = "previous route" (`Main.qml:146-153`) reads as browser Back; desktop users expect it to be unavailable or a text-caret action.

**Notifications page is off-system**
- Uses raw QtQuick.Controls and hardcoded pixels (`margins: 24`, `font.pixelSize: 24`, `NotificationsPage.qml:15-27`) while every other page uses QindaQt.Controls + Tokens — visibly different spacing/typography; its "Apply my choice" conflict button (`:84`) is unexplained jargon.

**Localization inconsistency**
- Model status strings are untranslatable `QStringLiteral` while QML uses `qsTr`: `appearance_settings_model.cpp:108-120` ("Loading appearance settings…", "Appearance changed elsewhere; current values reloaded"), `customize_settings_model.cpp:119-135`, `customize_settings_actions.cpp:205-410` ("Layout move committed"). Either route through `tr()` or move display text into QML.

**Destructive-action policy is inverted**
- Bluetooth "Forget" (unpairs a device) fires immediately with no confirmation (`BluetoothDeviceSection.qml:124-137`) while the reversible clipboard-history toggle sits behind a modal (`ClipboardPage.qml:275-350`); "Remove assignment" in Color is also unguarded (`ColorProfileSection.qml:47-61`). Clipboard's modal is the pattern to copy; apply it to Forget/Remove.

**No tooltips or help anywhere**
- `ToolTip`/`WhatsThis` grep across all Settings QML returns zero results; every explanation is an always-visible caption, inflating page height and forcing the jargon of finding 1 into permanent view. Tooltips (and collapsing long descriptions behind them) is the native place for the explanatory text these pages currently print inline (e.g., Color's permanent "stored intents" card, `ColorPage.qml:104-110`).

**Good patterns to preserve and generalize** (so repairs don't regress them): Display's revert-countdown confirmation banner; Clipboard's clear-history modal; ColorImportSection's FileDialog; the PageUp/PageDown/Ctrl+Home/End scroll handling and focus-reveal logic (`AppearancePage.qml:24-45,135-170`); the shared StateCard→form→footer page skeleton; Customize's real keyboard editing vocabulary (needs visible hints, `CustomizePage.qml:30-81`).

---

## Overlap with active repair lanes

- Services owners: findings 1 (reason codes), 6 (degraded-state contract) — the presentation side needs their recovery truth, but the strings/labels above are fixable in the app layer now.
- Navigation/appearance owners: findings 4 (Appearance pickers), 5 (Appearance/Display scale duplication), 7 (commit model), 8, plus the navigation-chrome notes.
- Customize owner: finding 2, Customize items in 5 and 7, customize-specific notes above.
- Global menu flash (finding 3) is shell-side (`src/shell/global_menu/**`), not Settings — flagged here because the user perceives it as part of the Settings session.

— finish-ux-glm, 2026-09-06. Read-only audit; no product files changed.
