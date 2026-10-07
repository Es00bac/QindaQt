# Everyday desktop audit — 7 October 2026

QindaQt's next improvement should be completing everyday journeys: return to
an unlocked desktop, use a USB drive, join a network, install an application,
recover a document, and let an agent help with a clearly chosen task. Much of
the underlying software exists. Integration, discovery, recovery, and installed
verification are less complete than the number of implemented modules suggests.

This is an **audit and proposed delivery plan, not a release acceptance**.
The owner requested documentation only. No product code, packages, live
settings, devices, passwords, or session state were changed by this audit.
Implementation belongs to later separately assigned workers. See the
[delivery plan](everyday-desktop-plan.md) for task boundaries and model choices.

## Evidence boundary

- Desktop source baseline: `f298b680375fd6d759b877803e8a88d71346d5d8` from
  qinda's `container-wm` hub. The laptop's unrelated showcase checkout is not
  the product baseline and was preserved.
- Installed laptop records: Desktop r13, compositor r6, QindaTK r15,
  Removable Media `0.1.0_p20260930-r2`, QQ_Term `0.3.0`, Office
  `0.1.0_p20261004`, and QindaPortage `0.1.0_p20260928-r4`.
- The October 7 lock policy is installed. Battery reporting recovered through
  normal UPower activation and boot enablement. The Power1 activation repair
  is integrated source, awaiting a desktop package. A real physical password
  unlock has not been observed. The earlier keyring attachment repair is also
  integrated source awaiting delivery; an existing r14 recipe must not be
  assumed to include subsequent Power1 changes. See the current task/handoff
  receipts and [native locking](../architecture/native-session-lock.md).
- Read-only inspection covered source, owning wiki pages, selected installed
  package/application records and process presence. It did not replay physical
  USB, suspend, authentication, printer, radio, or private-document journeys.
  Historical test results are attributed to their recorded boundary; this
  audit did not rerun product tests.

Labels below matter: **confirmed limitation** means the current source or
contract explicitly excludes the workflow; **reported** means the owner's
experience still needs a bounded reproduction; **qualification gap** means
implementation exists but the complete installed journey is not established;
**discovery gap** means an existing capability is hard to find or poorly joined;
**unassessed** is not a defect claim. A running process proves presence, not
successful interaction. An old roadmap caveat is not evidence of missing code.

## The standard we are aiming for

Every routine supported task should have a discoverable graphical entry,
keyboard access, useful progress, an understandable failure, and a recovery
action. A terminal remains a first-class choice. It must not be the recovery
instruction for ordinary failures such as a missing service or an unfamiliar
mount path. Advanced administration can expose an optional diagnostic command
without making users run it to complete the advertised task.

This is a workflow comparison, not a claim that every competing desktop is
flawless. Dolphin already places attached media and unmount actions in its
Places panel and offers a directory-aware terminal panel. That is a concrete
benchmark for joining files, devices and terminal work.
[Dolphin handbook](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/panels.html).
GNOME documents a graphical hidden-network connection flow, another useful
baseline for what users reasonably expect from Network settings.
[GNOME help](https://help.gnome.org/gnome-help/net-wireless-hidden.html).

## Findings by everyday journey

| Journey | Current evidence and gap | Priority and next outcome |
| --- | --- | --- |
| Return after idle; unlock; wake after closing the lid | **Reported failure, partly repaired; qualification gap.** Missing PAM policy explained the absent prompt. Policy installation and private regressions pass; physical focus, password entry, retry and wake remain open. Earlier greeter-frame proof did not prove authentication. | **P0 / ED-01:** qualify the installed login–idle–unlock–sleep cycle and package prerequisites. A frozen lock is a release blocker. |
| Know whether it is safe to unplug the laptop | **Reported failure, recovered; delivery gap.** Kernel battery data existed while UPower was dormant. Live reporting returned, but robust dormant-service activation remains source-only. Low/critical alerts and AC transitions also need installed hardware evidence. | **P0 / ED-02:** deliver the accepted startup repair, then prove fresh-session and AC/battery behavior. Do not show unknown charge as zero. |
| Open Mail or another application that needs saved credentials | **Open incident.** Native Secret Service and migration exist. A late daemon could lose session attachment; a permanent source repair is accepted, while real Mail authentication and physical PAM automatic unlock are still open. | **P0 / ED-03:** finish delivery and the owning-window prompt/readiness journey using the native keyring. Do not substitute another secret store. |
| Plug in, open, and eject a thumb drive | **Reported friction; discovery/integration gap.** The installed, resident Removable Media helper already offers Mount/Open, choices, encrypted unlock and safe removal. File Manager's own volume sidebar is deferred. The presence of the helper does not resolve the reported experience. | **P1 / ED-04:** one consistent device entry in File Manager and the chooser, with mount/open/eject, busy reasons and confirmed safe-to-unplug state. Reproduce the current insertion path first. |
| Move files onto a drive; delete and restore them | **Confirmed limitations.** Local Move refuses cross-filesystem destinations; home Trash refuses different-device items; recursive Copy rejects symbolic links. These are intentional safety boundaries with visible everyday consequences, not evidence of corruption. | **P1 / ED-05, ED-06:** safe cross-device moves and per-volume Trash, then explicit symlink-copy semantics. Source deletion must follow verified copy completion. |
| Browse a server or open an archive | **Existing capability with bounded gaps.** SMB/SFTP browsing, credentials, remote open and several mutations exist. Remote Trash is excluded. Archive creation/extraction delegates to an installed handler. The handbook's local-only and no-archive wording was stale. | **P1 / ED-07:** expose supported remote actions, truthful unsupported actions and graphical installation/recovery for a missing handler; test interrupted transfers. Do not rebuild network browsing or an archive engine. |
| Connect away from home | **Confirmed limitations.** Visible Open/WPA2/WPA3 Personal first-use exists. Hidden and enterprise first-use are unsupported in Network settings; there is no profile editor in that route, and the secret agent excludes VPN secrets and certificate selection. | **P1 / ED-08:** split hidden-network entry, saved-profile editing, enterprise/certificate setup and VPN into separate outcomes. Captive-portal behavior is **unassessed**, requiring reproduction before a feature claim. |
| Pair headphones; select a microphone; make a call | **Qualification gap.** Bluetooth pairing/trust and Audio device/stream controls exist. Live radio interoperability, audio profile transitions, headset reconnect and microphone/screen-share journeys need hardware/application evidence. | **P1 / ED-09:** qualify a small real-device matrix and repair observed failures. Do not describe pairing or screen sharing as wholly absent. |
| Dock a laptop; change scale; unplug a monitor | **Qualification gap.** Display arrangement, scale, preview/revert, brightness and night light exist. Private nested evidence is not physical mixed-DPI, dock, lid or suspend evidence. | **P1 / ED-09:** prove usable focus/panels/windows after dock and hotplug, and automatic recovery from an unconfirmed display change. |
| Read, copy text from, search and print a PDF | **Confirmed limitation.** The default Viewer renders images/PDFs but explicitly lacks text selection/copy, search, printing, forms and annotations. Text Editor and native Print portal support already exist. | **P1 / ED-10:** basic PDF select/copy/search/print first; forms/annotations later or a clearly supported alternative viewer. |
| Add a printer or scan a document | **Discovery/qualification gap; scanner unassessed.** CUPS and a Manage Printing desktop entry are installed on the laptop. Native Print portal and Text Editor printing exist. There is no Printers route in the built-in Settings registry. No scanner workflow was established by this audit. | **P1 / ED-11:** discover and test existing CUPS setup/job UI, integrate a friendly entry, and select/reuse a packaged scanning application before considering new code. |
| Install an app and keep the computer current | **Existing GUI; completeness unassessed.** QindaPortage is installed and supports package inspection, previewed plans, privileged apply and configuration revert. It is not absent. Its configuration snapshot is not proof of complete package, boot or data rollback. | **P1 / ED-12:** qualify a simple install/update/remove journey without requiring an LLM; expose progress, failures, CONFIG_PROTECT decisions and restart state. Reuse Portage and its authority. |
| Recover after a crash or a bad update | **Partial implementation.** Text Editor has bounded dirty-content recovery; QindaBoot offers older kernels. Signed package snapshots and incident archives exist. None establishes a general graphical personal-file backup/restore or complete system rollback journey. | **P1 / ED-13:** inventory existing recovery first; deliver one tested document restore and one clearly bounded package recovery workflow. Never label configuration-only revert “system rollback.” |
| Use the desktop without a mouse or with assistive technology | **Confirmed partial scope; qualification gap.** Contrast, text scale and reduced motion/transparency work; the screen-reader preference is reserved without a consumer. OSK has five layouts with US fallback. Full live AT-SPI traversal is unqualified. | **P1 / ED-14:** prove keyboard and screen-reader journeys across login, shell, files and settings; plan IME/localization independently from keyboard layout. |
| Change language, manage users, or complete first setup | **Confirmed partial scope.** Date/time can change zone and NTP, but locale is read-only and documentation directs users to `localectl`. No account-management route appears in the 23-route registry. Welcome exists but emphasizes grouping/customization. | **P2 / ED-15:** supported locale/account flows and task-based onboarding; accurately state the supported audience until these are qualified. |
| Use the terminal alongside graphical apps | **Substantial implementation.** QQ_Term has PTY lifecycle, literal argv execution, working-directory handoff, bracketed paste, search and job-close consent; File Manager has Open Terminal Here. No audit evidence justifies rewriting it. | **P1 regression requirement / ED-16:** preserve terminal parity in each new service and prove shell/SSH/full-screen terminal apps, quoting, Unicode and GUI file handoff. |
| Let an AI understand and help with current work | **Substantial app-level foundation; incomplete coverage.** QindaTK has an agent SDK and CLI/MCP; Calc/Note have semantic providers with document grants. Window commands, capture/input portals and browser integration also exist. A single scoped desktop-context workflow has not been established. | **P1 design / ED-17, ED-18:** reuse these boundaries, expose only chosen context, expand providers, and give users visible grants/revoke and action receipts. |
| Find an option and recover from “unavailable” | **Cross-cutting discovery/consistency gap.** Settings search and many Retry paths exist. Some handbook and roadmap summaries lag newer code; service errors can still leave the user needing operator intervention. | **P1 / ED-19:** correct current guides, connect errors to safe graphical recovery, and run task-based newcomer sessions. |

P0 protects access and working state. P1 completes the core daily-use promise.
P2 broadens the supported audience. These are audit priorities, not weighted
feature-ledger progress or claims that all P1 work must be newly implemented.

## Added owner requirement: Android and Windows applications

The owner wants Android apps in resizable ordinary windows with **green**
identification and Wine/Proton Windows apps with **blue** identification, all
cleanly listed in Applications. Installed Waydroid and Wine/Proton packages
and QindaLutris are existing foundations, not proof of this joined experience.
The [dedicated plan](foreign-app-integration-plan.md) records ED-20–24 for
runtime adapters, app identity, appearance, files/permissions and qualification.
The implementation must honor the same terminal-optional standard and reuse
the public application catalog rather than create a parallel launcher.

## Source evidence for the findings

Paths below are relative to their named repository at the recorded source
boundary. Detailed audit receipts live in
`ops/team/messages/everyday-desktop-audit-20261007/`.

| Evidence | Source or owning contract |
| --- | --- |
| Lock, power and keyring incidents | `docs/HANDOFF.md`, `docs/TASK_LIST.md`; [native lock](../architecture/native-session-lock.md), [Power1](../architecture/power-service.md), [native keyring](../architecture/keyring-daemon.md) |
| Removable-media authority and separate helper | [Removable media](../apps/removable-media.md), [ADR-0315](../adr/0315-session-owned-removable-media-through-udisks.md), `src/apps/removable_media/`, session supervision |
| Filesystem limits and remote support | [File Manager](../apps/file-manager.md); `src/apps/file_manager/mutation/local_mutation_backend.cpp`, `home_trash.cpp`, `safe_tree_operations.cpp` and the storage auditor's exact paths/anchors |
| Hidden networks and profile scope | [Network settings](../apps/network-settings.md), [secret agent](../architecture/network-secret-agent.md); `src/services/network_model/src/network_intent_policy.cpp` rejects hidden networks; `src/services/network_manager_adapter/src/libnm_visible_network.cpp` |
| Hardware scope | [Bluetooth settings](../apps/bluetooth-settings.md), [Audio](../apps/audio-settings.md), [Display](../apps/display-settings.md), [testing harness](testing-harness.md) and installed handoff |
| PDF and print | [Viewer](../apps/viewer.md), [Text Editor](../apps/text-editor.md#printing), [native Print](../reference/portal-misc-families.md); installed `cups.desktop` opens the CUPS web interface |
| Existing package UI | `LocalAiPortage` hub `7d9bde75b722081a09d59ae0dadd0f176f1da392`, `README.md`, `docs/design.md`, `docs/helper.md`; installed r4 pins `835f6046e6293261666d24b4865fea810bedab35` |
| Recovery limits | [Text Editor crash recovery](../apps/text-editor.md#crash-recovery-autosave); `QindaRecoveryBackup` hub `ed3d2028c411c7f91825ed7ec9a8c8760ece9c2a` README describes metadata/committed-source backups, excluding dirty trees and credentials; `QindaBoot` README describes older-kernel selection |
| Accessibility and locale | [Accessibility](../apps/accessibility-settings.md), [OSK](../apps/on-screen-keyboard.md), [Date/time](../apps/datetime-settings.md); `src/apps/settings_center/settings_route_registry.cpp` |
| Terminal and agents | [Terminal](../apps/terminal.md), [application SDK](../architecture/module-boundaries.md); `QindaTK` main `393c1ce5`, `QindaOffice` main `f1f3492b`, exact access-audit receipt and their own agent contracts |

## What not to conclude

This desktop does not need a replacement file manager, package manager,
terminal, keyring or universal AI protocol just because a workflow is awkward.
Reuse the maintained owner first. Existing safety refusals must gain safe
implementations, not be removed to make a demo pass. Default off agent access
and refusing an unsafe disk operation are not usability defects themselves;
missing explanation, discovery or a supported completion path can be.

Do not claim all Linux applications work from a portal unit test. Third-party
Qt/GTK/Electron/Flatpak applications, browsers, password prompts, file pickers,
notifications, camera/microphone and screen sharing need representative
installed journeys. Likewise, private virtual hardware proves only its stated
boundary. The plan includes those application and physical checks.

The first credible wider release is a bounded beta with a published supported
hardware/application matrix and known limitations. Two familiar machines and
hundreds of unit tests do not establish a general installer, migration path,
multi-user desktop or hardware support promise.
