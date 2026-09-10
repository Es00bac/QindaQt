# Architecture decision records

Architecture decision records (ADRs) preserve durable choices and their
tradeoffs so future agents do not have to reconstruct them from code. The
[documentation policy](../contributing/documentation-policy.md) defines when to
create or supersede one; start from the [ADR template](template.md).

| ADR | Status | Decision |
| --- | --- | --- |
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
| [ADR-0041](0041-adopt-flow-team-delivery-loop.md) | Accepted | Use workgroup queues, exact review loops, prompt integration, and capacity refill |
| [ADR-0042](0042-launcher-model-without-execution.md) | Accepted | Keep the launcher a pure model whose launch intents never execute |
| [ADR-0044](0044-inject-task-list-facts-into-the-shell.md) | Accepted | Inject immutable task-list facts rather than importing compositor internals |
| [ADR-0031](0031-volatile-bounded-clipboard-history.md) | Accepted | Keep the clipboard history volatile, bounded, and fail-closed |
| [ADR-0043](0043-isolate-the-customization-editor-domain.md) | Accepted | Isolate the customization editor domain as its own module |
| [ADR-0033](0033-canonical-menu-model-and-authenticated-menu-ownership.md) | Proposed | Own a canonical menu model with authenticated active-window menu ownership |
| [ADR-0032](0032-status-notifier-exact-owner-foundation.md) | Accepted | Key the status-notifier tray on exact unique-name owners |
| [ADR-0047](0047-pure-font-catalog-and-preference-boundary.md) | Accepted | Pure Font F0 catalog, preference, and bootstrap boundary |
| [ADR-0030](0030-confine-qtermwidget-behind-terminal-adapter.md) | Superseded by ADR-0030 | Confine the qtermwidget6 VT/rendering dependency behind the Terminal rendering adapter |
| [ADR-0040](0040-own-terminal-child-pty-and-bridge-through-teletype.md) | Accepted | Own the Terminal child PTY and bridge it through the qtermwidget teletype |
| [ADR-0037](0037-keep-pairing-and-trust-authority-in-bluez.md) | Accepted | Keep Bluetooth pairing and trust authority in BlueZ; defer Agent1 pairing |
| [ADR-0045](0045-fence-network1-pure-boundary.md) | Accepted | Fence Network1 owner, lineage, lease, secret, and pure-module contracts |
| [ADR-0046](0046-display-color-c0-model-boundary.md) | Proposed | Keep display color as a pure bounded model first |
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

Numbers are never reused, including for rejected or superseded records. A gap
may be reserved by another coordinated outcome and is not available for reuse;
integration retains every accepted decision in numeric order.

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
