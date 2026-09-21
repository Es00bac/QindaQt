# Architecture decision records

Architecture decision records (ADRs) preserve durable choices and their
tradeoffs so future agents do not have to reconstruct them from code. The
[documentation policy](../contributing/documentation-policy.md) defines when to
create or supersede one; start from the [ADR template](template.md).

| ADR | Status | Decision |
| --- | --- | --- |

Numbers are never reused, including for rejected or superseded records. A gap
may be reserved by another coordinated outcome and is not available for reuse;
integration retains every accepted decision in numeric order.










| [ADR-0001](0001-use-kwin-as-compositor-base.md) | Accepted | Use a small downstream KWin integration as the compositor base |
| [ADR-0002](0002-native-qindaqt-applet-api.md) | Accepted | Define a native, capability-declared QindaQt applet API |
| [ADR-0003](0003-docs-as-code.md) | Accepted | Maintain the project wiki and ADRs as repository source |
| [ADR-0004](0004-process-local-hybrid-topology.md) | Accepted; chrome portion superseded | Keep Hybrid topology process-local and compose member plus shared chrome |
| [ADR-0005](0005-scene-resident-hybrid-chrome.md) | Accepted | Render shared Hybrid chrome as a member-anchored scene item |
| [ADR-0006](0006-profile-global-applet-identity.md) | Accepted | Make applet instance identity global within a layout profile |
| [ADR-0007](0007-layer-shell-panel-surfaces.md) | Accepted | Use LayerShellQt behind a QindaQt panel-surface boundary |
| [ADR-0008](0008-lean-notification-service.md) | Accepted | Own a bounded QtDBus notification service without Plasma runtime |
| [ADR-0009](0009-use-kglobalaccel-for-shell-shortcuts.md) | Accepted | Use KGlobalAccel for user-remappable shell-wide shortcuts |
| [ADR-0010](0010-inject-shell-notification-interruption-policy.md) | Accepted; lifetime/UI clause superseded | Inject notification interruption policy on the shell side |
| [ADR-0011](0011-gate-notifications-on-authenticated-lock-state.md) | Accepted | Gate full notification presentation on owner/PID-authenticated KWin lock state |
| [ADR-0012](0012-persist-notification-quieting-through-settings1.md) | Accepted | Persist notification quieting through an activatable Settings1 authority |
| [ADR-0013](0013-own-qst1-semantic-tokens.md) | Accepted | Own QST-1 derivation and isolate optional Kirigami reuse behind adapters |
| [ADR-0014](0014-confine-wireplumber-to-glib-worker.md) | Accepted | Confine libwireplumber/GObject ownership to a dedicated GLib worker |
| [ADR-0015](0015-qualify-function-before-resource-refinement.md) | Accepted | Qualify the isolated virtual desktop before tightening its initial resource ceiling |
| [ADR-0016](0016-display1-transaction-authority.md) | Accepted | Make Display1 the QindaQt display-transaction authority while KWin owns live state and restore |
| [ADR-0017](0017-persistent-output-identity.md) | Accepted | Derive privacy-preserving persistent output identities with explicit ambiguity |
| [ADR-0019](0019-restart-the-production-shell-once.md) | Superseded | Restart the production shell once per compositor session |
| [ADR-0020](0020-authenticate-private-live-evidence.md) | Accepted | Authenticate a private, read-only live-session evidence boundary |
| [ADR-0021](0021-isolate-controls-visual-rows.md) | Accepted | Isolate every Controls visual row in its own process |
| [ADR-0022](0022-keep-text-documents-local-and-atomic.md) | Accepted | Keep Text Editor documents local, optimistic, and atomically persisted |
| [ADR-0023](0023-split-power-authority-across-service-and-shell.md) | Accepted | Split platform power observation from shell-owned session-action authority |
| [ADR-0024](0024-route-brightness-through-power1.md) | Accepted; write route superseded by ADR-0024 | Route fail-closed internal brightness through a Power1 provider |
| [ADR-0025](0025-arbitrate-session-bound-power1-activation.md) | Accepted | Arbitrate session-bound Power1 activation without reciprocal takeover |
| [ADR-0026](0026-contain-virtual-desktop-qualification.md) | Accepted | Contain integrated virtual desktop qualification in a private namespace and exact stage |
| [ADR-0027](0027-extract-a-narrow-first-party-application-shell.md) | Accepted | Extract a narrow first-party application shell without domain or platform authority |
| [ADR-0028](0028-compose-appearance-settings-through-settings1.md) | Accepted | Compose the Appearance settings route through Settings1 and QST-1 |
| [ADR-0029](0029-file-manager-bounded-local-launch.md) | Accepted | Open File Manager files through a bounded local launch intent |
| [ADR-0030](0030-confine-qtermwidget-behind-terminal-adapter.md) | Superseded by ADR-0030 | Confine the qtermwidget6 VT/rendering dependency behind the Terminal rendering adapter |
| [ADR-0031](0031-volatile-bounded-clipboard-history.md) | Accepted | Keep the clipboard history volatile, bounded, and fail-closed |
| [ADR-0032](0032-status-notifier-exact-owner-foundation.md) | Accepted | Key the status-notifier tray on exact unique-name owners |
| [ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md) | Proposed | Own a canonical menu model with authenticated active-window menu ownership |
| [ADR-0037](0037-keep-pairing-and-trust-authority-in-bluez.md) | Accepted | Keep Bluetooth pairing and trust authority in BlueZ; defer Agent1 pairing |
| [ADR-0040](0040-own-terminal-child-pty-and-bridge-through-teletype.md) | Accepted | Own the Terminal child PTY and bridge it through the qtermwidget teletype |
| [ADR-0041](0041-adopt-flow-team-delivery-loop.md) | Accepted | Use workgroup queues, exact review loops, prompt integration, and capacity refill |
| [ADR-0042](0042-launcher-model-without-execution.md) | Accepted | Keep the launcher a pure model whose launch intents never execute |
| [ADR-0043](0043-isolate-the-customization-editor-domain.md) | Accepted | Isolate the customization editor domain as its own module |
| [ADR-0044](0044-inject-task-list-facts-into-the-shell.md) | Accepted | Inject immutable task-list facts rather than importing compositor internals |
| [ADR-0045](0045-fence-network1-pure-boundary.md) | Accepted | Fence Network1 owner, lineage, lease, secret, and pure-module contracts |
| [ADR-0046](0046-display-color-c0-model-boundary.md) | Proposed | Keep display color as a pure bounded model first |
| [ADR-0047](0047-pure-font-catalog-and-preference-boundary.md) | Accepted | Pure Font F0 catalog, preference, and bootstrap boundary |
| [ADR-0048](0048-settings-center-navigation-and-route-ownership.md) | Accepted | Keep Settings navigation typed and route authority local |
| [ADR-0049](0049-capture-private-parent-framebuffer.md) | Accepted | Capture one private Weston parent framebuffer after private-seat interaction |
| [ADR-0050](0050-direct-kde-output-management-writer.md) | Accepted | Use a direct bounded KDE public output-management writer |
| [ADR-0051](0051-persist-display-journal-in-injected-state-root.md) | Accepted | Persist canonical Display1 recovery truth in one injected user-state directory |
| [ADR-0052](0052-confine-networkmanager-behind-network1.md) | Accepted | Confine libnm, credentials, and upstream-owner replacement behind resident Network1 |
| [ADR-0053](0053-compose-display1-from-authenticated-runtime-authorities.md) | Accepted | Compose Display1 from explicit journal, Wayland-peer, lock, writer, and logind authorities |
| [ADR-0054](0054-export-appearance-through-the-standard-settings-portal.md) | Accepted | Export confirmed Settings1/QST appearance through the standard Settings portal backend |
| [ADR-0055](0055-compose-network-settings-through-network1.md) | Accepted | Compose Network settings through public Network1 without credential authority |
| [ADR-0056](0056-adopt-standard-appmenu-dbusmenu-transports.md) | Accepted | Adopt standard AppMenu/dbusmenu transports behind proof-bound ownership |
| [ADR-0057](0057-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md) | Accepted | Reach BlueZ through injected direct QtDBus while preserving BlueZ authority |
| [ADR-0058](0058-isolate-clipboard-capture-in-a-volatile-host.md) | Accepted | Isolate clipboard capture and payloads in a volatile resident host |
| [ADR-0059](0059-route-unimplemented-portal-families-explicitly.md) | Accepted | Route unimplemented portal families through an explicit fail-closed table |
| [ADR-0060](0060-confine-production-power-upstreams.md) | Accepted | Confine production power upstreams behind injected adapters |
| [ADR-0061](0061-authenticate-shell-window-actions-by-panel-owner.md) | Accepted | Authenticate shell window actions by the exact panel Wayland owner PID |
| [ADR-0062](0062-bound-launcher-execution-behind-injected-seams.md) | Accepted | Bound launcher execution behind injected seams |
| [ADR-0063](0063-project-authenticated-active-window-identity.md) | Accepted | Project authenticated active-window identity to the exact shell owner |
| [ADR-0064](0064-confine-file-mutation-to-identity-checked-local-authority.md) | Accepted | Confine File Manager mutation to identity-checked local authority |
| [ADR-0065](0065-persist-text-editor-path-inventory.md) | Accepted | Persist only Text Editor's bounded path inventory |
| [ADR-0066](0066-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md) | Accepted | Discover ICC profiles from injected roots and persist assignments through Settings1 |
| [ADR-0067](0067-confine-fontconfig-behind-font-discovery.md) | Accepted | Confine fontconfig behind the Font F1 discovery provider |
| [ADR-0068](0068-compose-first-party-menu-export-through-appshell.md) | Accepted | Compose first-party menu export through an opt-in AppShell transport boundary |
| [ADR-0069](0069-confine-network-credential-entry.md) | Accepted | Confine network credential entry to a separate secret agent |
| [ADR-0070](0070-confine-session-actions-behind-authenticated-boundaries.md) | Accepted | Confine logout, lock, and machine power actions behind authenticated boundaries |
| [ADR-0071](0071-publish-and-prove-shell-token-readiness.md) | Accepted | Publish QST-1 before shell QML and prove readiness at boot |
| [ADR-0072](0072-shell-iconography-confined-xdg-icon-themes.md) | Accepted | Shell iconography over confined XDG icon themes |
| [ADR-0073](0073-publish-atomic-authenticated-task-facts.md) | Accepted | Publish atomic task facts only to the authenticated production shell |
| [ADR-0074](0074-compose-shell-preferences-through-settings1.md) | Accepted | Compose shell layout and token preferences through Settings1 |
| [ADR-0075](0075-desktop-controls-and-workspaces.md) | Proposed | Compose desktop controls over bounded shell facades |
| [ADR-0076](0076-register-launcher-persistence-in-panel-settings.md) | Accepted | Register launcher persistence in panel settings |
| [ADR-0077](0077-acknowledge-global-menu-hosting-before-hiding-local-menus.md) | Accepted | Acknowledge global menu hosting before hiding local menus |
| [ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md) | Accepted | Own wallpaper surfaces in the shell |
| [ADR-0079](0079-own-welcome-presentation-preference-locally.md) | Accepted | Own Welcome preference locally and supervision in session |
- [ADR-0080: Resolve first-party appearance from confirmed Settings1 preferences](0080-resolve-first-party-appearance-from-settings.md)
- [ADR-0081: Project confirmed appearance into native and grouped chrome](0081-project-confirmed-appearance-into-window-chrome.md)
- [ADR-0082: Publish the current session activation environment](0082-publish-session-activation-environment.md)
- [ADR-0083: Apply saved color profiles through public output management](0083-apply-saved-color-profiles-through-public-output-management.md)
- [ADR-0084: Own the desktop shortcut note on wallpaper surfaces](0084-own-the-desktop-shortcut-note-on-wallpaper-surfaces.md)
- [ADR-0085: Pre-empt KWin's native custom-tile from an early, narrow input filter](0085-early-late-shift-takeover-filter.md)
- [ADR-0086: Route GlobalShortcuts only to a verified backend](0086-route-globalshortcuts-only-to-a-verified-backend.md)
- [ADR-0087: Deliver agent and Gabbee input through the RemoteDesktop portal](0087-agent-input-via-remotedesktop-portal.md)
- [ADR-0088: Scope the KDE portal compatibility identity to its backend process](0088-enable-kde-remote-desktop-for-qindaqt.md)
- [ADR-0089: Present task switching through KWin's native model](0089-present-task-switching-through-kwins-native-model.md)
- [ADR-0090: Keep File Manager bookmarks in an app-local bounded state file](0090-keep-file-manager-bookmarks-app-local.md)
- [ADR-0091: Configure KScreenLocker preferences through a narrow Settings adapter](0091-configure-kscreenlocker-preferences-through-settings.md)
- [ADR-0092: Project the confirmed palette into QindaQt compositor UI](0092-project-confirmed-palette-into-compositor-ui.md)
- [ADR-0093: Acknowledge agent input only after portal acceptance](0093-acknowledge-agent-input-portal-acceptance.md)
- [ADR-0094: Refresh resident Wayland-connected services at session entry](0094-refresh-resident-wayland-session-services.md)
- [ADR-0095: Use KSyntaxHighlighting for editor presentation](0095-use-ksyntaxhighlighting-for-editor-presentation.md)
- [ADR-0096: Package bundled applications with Portage](0096-package-bundled-apps-with-portage.md)
- [ADR-0097: Separate workspace slots from live windows](0097-separate-workspace-slots-from-live-windows.md)
- [ADR-0098: Gate releases on the exact native compositor stack](0098-gate-releases-on-the-exact-native-compositor-stack.md)
- [ADR-0099: Shade containers by hiding member content](0099-shade-whole-containers-by-hiding-member-content.md)
- [ADR-0100: Own desktop essentials in a session process](0100-own-desktop-essentials-in-a-session-process.md)
- [ADR-0101: Launch workspace applications through desktop entries](0101-launch-workspace-apps-through-desktop-entries.md)
- [ADR-0102: Adopt restored layouts atomically](0102-adopt-restored-layouts-atomically.md)
- [ADR-0103: Preserve the compositor session during shell recovery](0103-paced-shell-recovery.md)
- [ADR-0104: Serialize production shell rendering](0104-serialize-shell-rendering.md)
- [ADR-0105: Delegate idle display-off to PowerDevil](0105-delegate-idle-display-off-to-powerdevil.md)
- [ADR-0106: Accept an equivalent Gentoo Power Profiles provider](0106-accept-equivalent-power-profiles-provider.md)
- [ADR-0107: Delegate Print to Spectacle](0107-delegate-print-to-spectacle.md)
- [ADR-0108: Compose System Monitor from detachable views](0108-compose-system-monitor-from-detachable-views.md)
- [ADR-0109: Pearl and Smoked Plum materials](0109-use-pearl-and-smoked-plum-app-materials.md)
- [ADR-0110: Ordinary editor windows](0110-own-editor-documents-in-ordinary-windows.md)
- [ADR-0111: bound file previews and consume public icons](0111-bound-file-previews-and-consume-public-icons.md)
- [ADR-0112: terminal protocol palette](0112-terminal-protocol-palette.md)
- [ADR-0114: actionable status notifier menus](0114-status-notifier-actionable-menus.md)
- [ADR-0115: share appearance through Qt platform theme](0115-share-appearance-through-qt-platform-theme.md)
- [ADR-0116: build bundled applications on stock Qt 6](0116-build-bundled-applications-on-stock-qt6.md)
- [ADR-0117: veto native resize for container members](0117-veto-native-resize-for-container-members.md)
- [ADR-0118: user task-order overlay and panel quick settings](0118-user-task-order-overlay-and-panel-quick-settings.md)
- [ADR-0119: authenticated window-preview channel](0119-authenticated-window-preview-channel.md)
- [ADR-0120: panel translucency and blur](0120-panel-translucency-and-blur.md)
- [ADR-0121: real iconography in the Settings Customize route](0121-real-iconography-in-the-settings-customize-route.md)
- [ADR-0122: adopt saved layout preferences live](0122-adopt-saved-layout-preferences-live.md)
- [ADR-0123: a Voicemeeter-class audio graph on PipeWire primitives](0123-voicemeeter-class-audio-graph-on-pipewire.md)
- [ADR-0124: add the QindaQt Bliss Luna option set](0124-add-qindaqt-bliss-luna-option-set.md)
- [ADR-0125: host desktop-zone applets on the desktop surface](0125-host-desktop-zone-applets.md)
- [ADR-0126: ignore user-override entries the schema cannot normalize](0126-ignore-user-overrides-the-schema-cannot-normalize.md)
- [ADR-0127: preview window chrome and the Qt toolkit through one painter](0127-preview-window-chrome-and-toolkit-through-one-painter.md)
- [ADR-0128: add the Settings Accessibility route](0128-accessibility-settings-route.md)
- [ADR-0129: configure window and container chrome through Appearance](0129-configure-window-and-container-chrome.md)
- [ADR-0130: attach menus to windows when the layout has no global menu](0130-window-attached-menus-without-a-global-menu.md)
- [ADR-0131: contained windows keep a handlebar; the wheel rolls chrome up](0131-contained-window-handlebar-and-wheel-roll-up.md)
- [ADR-0132: finish session locking on KWin's locker and PowerDevil's actions](0132-finish-session-locking.md)
- [ADR-0133: route every portal family explicitly](0133-route-every-portal-family.md)
- [ADR-0134: own input and shortcut settings through KWin and kglobalaccel](0134-input-and-shortcut-settings.md)
- [ADR-0135: gnome-keyring is the Secret Service provider](0135-gnome-keyring-secret-service.md)
- [ADR-0136: night light through KWin and knighttimed](0136-night-light-through-kwin.md)
- [ADR-0137: File Manager network-location browsing (S5)](0137-file-manager-network-location-browsing.md)
- [ADR-0139: identity borders, focus emphasis, and the rolled-up badge](0139-identity-borders-focus-and-rolled-up-badge.md)
- [ADR-0148: admit internal-panel brightness through Power1](0148-admit-internal-panel-brightness-through-power1.md)
- [ADR-0150: admit immediate external-output brightness through Display1](0150-admit-immediate-external-output-brightness-through-display1.md)
- [ADR-0151: authenticated SMB/SFTP browsing through KIO's standard UI delegate](0151-file-manager-authenticated-network-browsing.md)
- [ADR-0152: remote regular-file opening through KIO's OpenUrlJob](0152-file-manager-remote-file-opening.md)
- [ADR-0153: same-folder remote Rename through KIO's rename()](0153-file-manager-remote-rename.md)
- [ADR-0154: remote New Folder through KIO's mkdir()](0154-file-manager-remote-new-folder.md)
- [ADR-0155: remote Copy To through KIO's copy()](0155-file-manager-remote-copy-to.md)
- [ADR-0156: remote Move To through KIO's move()](0156-file-manager-remote-move-to.md)
- [ADR-0157: remote write-in-place through the session KIOFuse service](0157-file-manager-remote-write-in-place.md)
- [ADR-0158: bootstrap one session bus before the compositor](0158-bootstrap-one-session-bus-before-the-compositor.md)
- [ADR-0159: author distinct decoration presets for every built-in theme](0159-author-distinct-decoration-presets-for-every-builtin-theme.md)
- [ADR-0160: select installed KWin window decorations explicitly](0160-select-installed-kwin-window-decorations.md)
- [ADR-0161: persist and mutate desktop icons behind owned boundaries](0161-persist-and-mutate-desktop-icons.md)
- [ADR-0162: container aspect-ratio lock](0162-container-aspect-ratio-lock.md)
- [ADR-0163: generated container names for the rolled-up badge](0163-generated-container-names-for-the-rolled-up-badge.md)
- [ADR-0164: one shared application catalog behind the file manager applications browser](0164-shared-application-catalog-and-file-manager-applications-browser.md)
- [ADR-0165: workspace picker slots replaced by launched applications](0165-workspace-picker-slots-replaced-by-launched-applications.md)
- [ADR-0166: announce a StatusNotifier host, not only a watcher](0166-announce-a-status-notifier-host.md)
- [ADR-0167: one desktop across every output](0167-one-desktop-across-every-output.md)
- [ADR-0168: a generated container name never displaces a real title](0168-a-generated-name-never-displaces-a-real-title.md)
- [ADR-0169: report the program behind an opaque window class](0169-report-the-program-behind-an-opaque-window-class.md)
- [ADR-0170: judge the KDE portal backend's start by exec, not by a bus name](0170-survive-a-private-session-bus-for-dbus-units.md)
- [ADR-0171: one gain law for the audio console](0171-one-gain-law-for-the-audio-console.md)
- [ADR-0172: Applications is a place, and a docked window can replace itself](0172-applications-is-a-place-and-a-docked-window-can-replace-itself.md)
- [ADR-0173: the mixing-console slice of Audio1](0173-the-mixing-console-slice-of-audio1.md)
- [ADR-0174: meters are a stream, not a snapshot](0174-meters-are-a-stream-not-a-snapshot.md)
- [ADR-0175: virtual strips and buses are nodes the console owns](0175-virtual-strips-and-buses-are-nodes-the-console-owns.md)
- [ADR-0176: the console remembers itself](0176-the-console-remembers-itself.md)
- [ADR-0177: pan is balance on the send](0177-pan-is-balance-on-the-send.md)
- [ADR-0178: a pin is a name, not a handle](0178-a-pin-is-a-name-not-a-handle.md)
- [ADR-0179: the rack is one value per strip](0179-the-rack-is-one-value-per-strip.md)
- [ADR-0180: a bus has a rack too, and a strip hears clean](0180-a-bus-has-a-rack-too-and-a-strip-hears-clean.md)
- [ADR-0181: the tray rides the console](0181-the-tray-rides-the-console.md)
- [ADR-0182: a preset is the console under a name](0182-a-preset-is-the-console-under-a-name.md)
- [ADR-0183: a macro button is a list of console operations](0183-a-macro-button-is-a-list-of-console-operations.md)
- [ADR-0184: the recorder is a stream and a writer thread](0184-the-recorder-is-a-stream-and-a-writer-thread.md)
- [ADR-0185: VBAN is a document and two threads](0185-vban-is-a-document-and-two-threads.md)
- [ADR-0191: write internal brightness through logind](0186-write-internal-brightness-through-logind.md)
- [ADR-0187: Smart lights speak to luminaires from the shell process](0187-smart-lights-speak-to-luminaires-from-the-shell-process.md)
- [ADR-0188: serve the panel start zone first](0188-serve-the-panel-start-zone-first.md)
- [ADR-0189: paint the rolled-up badge label, and size the badge to it](0189-size-the-rolled-up-badge-to-its-label.md)
- [ADR-0190: mirroring is one field on the mirrored output](0190-mirroring-is-one-field-on-the-mirrored-output.md)
- [ADR-0191: a control survives reprojection and owns its value](0191-a-control-survives-reprojection.md)
- [ADR-0192: the KDE portal drop-in must not try to clear `BusName=`](0192-portal-dropin-busname-cannot-be-cleared.md)
- [ADR-0193: a finger is the left button, and a held finger the right](0193-a-finger-is-the-left-button-and-a-held-finger-the-right.md)
- [ADR-0194: a saved network location is a name and a canonical address](0194-saved-network-locations-are-canonical-addresses.md)
- [ADR-0195: one owner per transfer, and a queue for the ones that cross the network boundary](0195-one-owner-per-transfer-and-a-queue-for-the-network.md)
- [ADR-0196: network sign-in belongs to the platform, not to QindaQt](0196-network-sign-in-belongs-to-the-platform.md)
- [ADR-0197: pen displays map themselves and ask once](0197-pen-displays-map-themselves-and-ask-once.md)
- [ADR-0198: File Manager preferences are app-local, exact, and every one of them does something](0198-file-manager-preferences-are-app-local.md)
- [ADR-0199: mount at login is a systemd user unit the file manager writes and nothing more](0199-mount-at-login-is-a-systemd-user-unit-the-app-writes.md)
- [ADR-0200: nearby servers are advisory, opt-in, and only what can be opened](0200-nearby-servers-are-advisory-and-opt-in.md)
- [ADR-0201: one obs-websocket client for the desktop](0201-one-obs-websocket-client-for-the-desktop.md)
- [ADR-0202: QindaQt provisions OBS and owns one secret](0202-qindaqt-provisions-obs-and-owns-one-secret.md)
- [ADR-0203: an ordinary window rolls up to its icon](0203-an-ordinary-window-rolls-up-to-its-icon.md)
- [ADR-0204: the on-screen keyboard is the compositor's input method](0204-the-on-screen-keyboard-is-the-compositors-input-method.md)
- [ADR-0205: touch edges and touch preferences belong to the compositor](0205-touch-edges-and-touch-preferences-belong-to-the-compositor.md)
- [ADR-0206: a theme authors surfaces, radii and motion, not only colors](0206-theme-schema-v2-surfaces-motion-and-decoration-themes.md)
- [ADR-0207: a decoration theme is its own document](0207-decoration-themes-are-their-own-documents.md)
- [ADR-0208: console buses are OBS sources](0208-console-buses-are-obs-sources.md)
- [ADR-0209: `windowManagement.*` is bridged into kwinrc by the session, live](0209-bridge-window-management-settings-into-kwinrc.md)
- [ADR-0210: add the Windows & workspaces Settings route over the live keys only](0210-windows-and-workspaces-settings-route.md)
- [ADR-0211: the clock and region page acts on the platform's own services](0211-the-clock-and-region-page-acts-on-the-platforms-own-services.md)
- [ADR-0212: quiet hours are a window the settings service owns](0212-quiet-hours-are-a-window-the-service-owns.md)
- [ADR-0213: host the customization editor live in the shell](0213-host-the-customization-editor-live-in-the-shell.md)
- [ADR-0214: the startup applications route shadows, never edits, system entries](0214-startup-applications-route.md)
- [ADR-0215: the idle screensaver is decoration, and never a lock](0215-the-idle-screensaver-is-decoration-not-a-lock.md)
- [ADR-0216: the locker draws the screensaver, so a locked session keeps showing it](0216-the-locker-draws-the-screensaver.md)
- [ADR-0226: configure the screen saver](0226-configure-the-screen-saver.md)
- [ADR-0218: use QindaTK and Poppler for the image/PDF viewer](0218-use-qindatk-and-poppler-for-the-viewer.md)
- [ADR-0219: share completed work through qinda](0219-share-completed-work-through-qinda.md)
- [ADR-0220: Rebuild the System Monitor on QindaTK as a dock dashboard](0220-rebuild-the-system-monitor-on-qindatk.md)
- [ADR-0221: One owner for panel popup placement](0221-one-owner-for-panel-popup-placement.md)
- [ADR-0222: QQ_Term is the desktop terminal](0222-qq-term-is-the-desktop-terminal.md)
- [ADR-0223: One stock profile per distinct feel](0223-one-stock-profile-per-distinct-feel.md)
- [ADR-0224: Name the presentations a layout asks for](0224-name-the-presentations-a-layout-asks-for.md)
- [ADR-0225: configure the login screen from Settings through a polkit-gated helper](0225-configure-the-login-screen.md)
- [ADR-0227: The audio console on QindaTK](0227-the-audio-console-on-qindatk.md)
- [ADR-0228: bundled wallpapers resolve beyond PNG](0228-bundled-wallpapers-resolve-beyond-png.md)
- [ADR-0229: proxy the legacy XEmbed tray into StatusNotifier items](0229-proxy-the-xembed-tray-into-status-notifier-items.md)
- [ADR-0230: name and picture the game behind a launcher class](0230-name-and-picture-the-game-behind-a-launcher-class.md)
