# QindaQt product task list

This is the outcome-oriented source of truth for active product work. It does
not count assignments, processes, reviews, or partially implemented code as
completion. Architectural detail and long-range milestone state remain in the
[implementation roadmap](wiki/development/implementation-roadmap.md).

## Active outcomes

### Make first-party applications visual and container-native (September 8)

**Implemented and app-qualified; Portage delivery pending.** Editor uses ordinary
single-document windows and independent menus/consent; Terminal retains one shell
per window. File Manager defaults to an icon grid with bounded real thumbnails,
concise navigation and original artwork. Pearl/Smoked Plum materials, branded
SVGs and readable light/dark/high-contrast controls are shared across the apps.
The native build with 24 jobs build,121 focused tests and four observed-scale native app
capture rows pass. Broader desktop failures and installation state are recorded
in [Handoff](HANDOFF.md); this does not declare the full desktop roadmap complete.
The September 7 per-tab descriptions below are historical checkpoints superseded
by this ordinary-window behavior.

### Let containers manage Terminal tabs and splits (September 7)

**Integrated, installation pending:** one shell per Terminal window. Removed Terminal's
internal tab strip, tab navigation, and tab-specific actions. New Terminal
opens a separate window with the current working directory/profile behavior;
QindaQt containers provide grouping, tabs, and splits. Keep search, copy/paste,
font, zoom, and the verified interactive PTY/prompt behavior. Install the change
through the maintained Gentoo package after focused regression checks.

### Correct the interactive Terminal and Gentoo installation (September 7)

**Installed:** the silent-bell parser bug is fixed at `89ad375d`. A full-window
interactive Bash regression verifies the prompt, keyboard input and visible
output on Wayland. Portage now owns the three bundled apps and shared runtime
files as `gui-apps/qindaqt-apps-0.1.0_pre20260907`, with declared dependencies and
installed RUNPATHs. This corrects the insufficient earlier normal-shell and
raw-copy installation checks. See [Handoff](HANDOFF.md) for evidence.

### Finish the bundled applications (September 7)

The user requests primary-assistant implementation, with no delegated coding.
File Manager S2 is integrated and installed: 22/22 tests plus the installed
UI-action probe pass. Stable selection repairs supersede the rejected original
candidate. Text Editor now has line numbers, syntax highlighting, line navigation,
indentation, per-tab wrapping and zoom. Terminal preserves its font on theme
changes, supports per-tab zoom, and opens tabs in the active shell’s folder.
**Installed and verified:** all 66 File Manager/Editor/Terminal rows pass on
main at `44fff730`. The installed Editor and Terminal launch in native Wayland;
Terminal’s shell owns a working PTY. Editor also passes its installed startup
and memory limits. This closes the daily-use app update, not the later File
Manager mounts/SMB roadmap.
Kimi is separately implementing Calendar; these changes do not own that app.


### Complete the installed desktop experience (2026-09-06)

**Completed as the bounded usability checkpoint on September 7, 2026.**
The reviewed changes are installed, the shell and services are refreshed,
and the closing suite passed 661/661. This closes the reported-task checkpoint,
not the broader desktop roadmap or File Manager S2.

| Requirement | Acceptance evidence |
| --- | --- |
| Stable menus and usable hit areas | Native lower-edge File/status-icon clicks, retained popups and exactly-once actions; full labels in `5b76fbf122f043bbae405a34fd055ba1`; installed shell refreshed. |
| Sloom global menu | Reviewed `f459b0fd`; actual Help → About and absent duplicate local menu in `a20d3aed75694d879a0a1cfab499c033`; complete installed package hash-verified. |
| Services, displays and color | Six refreshed services return snapshots; physical ICC apply/readback/removal; physical scale preview/Keep/Cancel; user confirms Samsung Settings 125%, independently read back with no pending transaction. |
| Focused Settings and QindaPunk design | Inspected Appearance, Color, Power and popup captures; compact navigation geometry tests plus native `d7a0ca34b3234d3ea1de20b892756ef8`; human guides and reviewed theme assets integrated. |
| Grouping and grouped geometry | Normal and late-Shift native runs `7bf2271d708648ee99e1afd80493ef40` and `d90a2e7ddd8747a38dea9e06dc106bb9`; container-local arrows with independent standalone tiling. |
| One container task and appropriate switching | Task identity regressions and native forward/reverse TabBox `5f700906669c498ab4889da621667fc7`. |
| Chrome, active frames and group iconify | Native title toggles, inspected context menu and minimize/restore in `085a865f3f804842b342c06f088e0213`; scoped container checks pass although that run's unsupported nested scale check fails. |
| Dock work area | Hidden/revealed/hidden maximized geometry and exact restore in `0c4d7868b85b44469417c4d28da7398a`. |
| Note and automatic locking | Native note hide/reopen with corrected shortcut; host Meta+Shift+F1 persisted; actual Power switch/15-minute selection and restoration in `5b76fbf122f043bbae405a34fd055ba1`; host idle locking remains disabled. |
| Gabbee and approved input | Real sink-to-PTY standalone/group Unicode readbacks; 187/187 Gabbee tests; real-backend push-to-talk and command callbacks each exactly once in their native runs. |
| Fullscreen and representative games input | Native fullscreen focus/restoration `0aca8a74926c44f98def9b87b2a83328` and pointer lock/confinement/relative motion `d8f380054b0c44fe8b176692063e6e11`. |
| Deployment and maintenance | 1,077 QindaQt and 3,181 Sloom entries independently match installed files; refreshed shell/services/Settings; compositor plugin matches the already refreshed session; reviewed integration pushed to GitHub, documentation gates pass. |

Qualification limits remain explicit: native private captures supply visual
checks because fresh host screenshot capture was unavailable. KWin's nested
Wayland backend ignores requested scale; the physical Settings path is verified
by the user's confirmation and live 1.25 readback. Synthetic dictation checks
cover delivery and shortcut routing, not physical microphone transcription or
every game. The user's Samsung remains at 125%; the projector remains at 100%.


The physical-session report supersedes the earlier visual-polish deferral.
These are concurrent requirements; later reports add to this list rather than
replace earlier work:

- Restore usable Display, Audio, Power, Bluetooth, Network, and Clipboard
  service states, including cold startup and recovery. Display/Clipboard
  activation and Audio/Power/Network/Bluetooth recovery code are integrated;
  the repaired Bluetooth fixtures and the full 100-test service selection pass.
- Detect connected displays, including disabled projectors, and offer usable
  enable/layout controls in Settings. Include visual monitor positioning,
  per-monitor scaling, mixed-resolution layouts, and understandable Apply/Keep/
  Revert behavior; raw coordinates alone are not the main arrangement workflow.
  The physical DP-1 projector is enabled beside HDMI-A-1; inventory and
  arrangement presentation repairs are integrated through `86342441`.
  The reported scale rollback is repaired through `347eb03f`: physical Display1
  preview/Cancel and Keep cycles reached 125% and restored the original layout.
  The verified service is deployed through a stable user-level systemd override;
  the user subsequently confirmed the physical Settings 125% change, and
  live readback verifies 1.25 with no pending transaction.
- Make maximized containers follow the available work area when the dock hides
  or changes its reservation, preserving their normal restore size. Integrated
  `04954961` passes actual hidden/revealed/hidden and exact restored-member
  geometry in run `0c4d7868b85b44469417c4d28da7398a`.
- Finish existing Color profile application and explain actual hardware
  capabilities without protocol jargon or nonfunctional controls. ICC application
  is integrated through `deb48cd3`; 13 focused checks pass. Physical KScreen application/readback/removal of a standard sRGB ICC profile
  passed and restored the original state (`.cache/live-color-proof/result.json`).
  The actual Color unavailable-state presentation was inspected in the
  native Settings run; physical ICC application evidence remains separate.
- Make Settings and the wider desktop human-friendly: focused Appearance
  destinations, clear selection and apply behavior, usable customization,
  consistent controls, compact navigation, and real visual verification.
- Rewrite the main user guides in clear, practical language: explain what
  people can do and what happens next, use consistent window-container terms,
  and keep contributor implementation details in contributor references. Start
  with getting started, desktop/window management, Settings, and applications.
- Develop the QindaPunk-inspired theme with original, reference-informed art
  direction. Use image generation across the design process where useful;
  preserve explicit wallpaper/theme choices. Palettes, shared editable controls,
  and reviewed Controls baselines are integrated through `3de6ac9f`; final
  Settings consumer review and native desktop visual acceptance are recorded
  in the checkpoint table above.
- Eliminate global-menu flashing and make pointer and keyboard actions reliable.
  The refreshed session still has undersized or vertically displaced click
  targets on menu labels and status icons. Integrated `8179a0f9` removes
  invisible scrollbar interception and expands usable hit areas; independent
  and integrated checks pass 20/20. Native run
  `b9959bfb91df43e49b5a5d4568aafcb4` verifies a measured 93 ms lower-edge
  File click, a popup retained after release and one second, and exactly one
  New action. Separate captures verify lower-edge Clipboard and Bluetooth
  opening. The new shell is installed and was refreshed at 08:33 MDT on September 7.
  Native-menu fixes are integrated through `71f3b49b`. In isolated run
  `de9f130640494649993e7001d75810a7`, inspected screenshots show a correctly
  anchored File popup staying open after release and a one-second wait, with
  exactly one new tab per pointer/keyboard New action. The semantic observer
  was unavailable, so the automated gate remains false. The earlier Tabs sizing
  repair `a631a02c` was insufficient. Integrated `575e30d2` now keeps admitted
  labels complete; native run `5b76fbf122f043bbae405a34fd055ba1` shows full
  Tabs text, and all 10 affected integrated CTest rows pass. The refreshed
  installed QindaQt session started at 20:17 MDT; live action verification
  remains separate from that installation proof.
- Offer automatic screen-lock configuration in Settings and respect the user's
  choice to disable idle locking. Integrated through `94b302ea`: Power settings
  has an automatic-lock switch and duration selector, preserving custom values
  and unrelated locker preferences; independent focused tests pass 7/7. The
  host automatic idle-lock preference is off. The updated Settings UI is installed
  in the refreshed desktop. Private run `5b76fbf122f043bbae405a34fd055ba1`
  exercises the real switch and 15-minute selection, then restores disabled/5
  minutes; physical host interaction remains separate.
- Support Sloom Studio by default through its existing Electron/custom-KDE
  global-menu integration. Its standard dbusmenu twin is exported at
  `/org/signalloom/menus/active` by `org.signalloom.PanelMenu`; preserve that
  existing contract and qualify real menu actions. The reviewed selector is
  integrated at `d2be019f`; focused Sloom/editor checks pass 2/2. Real installed
  Sloom exports the expected service from its Electron main process; panel
  consumption and Help → About now pass in private native run
  `a20d3aed75694d879a0a1cfab499c033`. The separately reviewed Sloom
  candidate `f459b0fd` also removes the duplicate local menu; its packaged
  production files match the accepted source. Host installation is complete and all packaged files were independently verified.
- Diagnose the reported ChatGPT launch segmentation fault and repair any
  established desktop/runtime integration defect without resetting user data.
  Resolved launch regression: isolated X11 launch reproduced SIGSEGV; native
  Wayland remained running. The user launcher now selects Wayland in a Wayland
  session, with the original preserved and a normal-launch survival check.
- Maintain the project on the user’s GitHub with reviewed integration commits,
  working CI dependencies, isolated worker branches, and ignored build/session
  output. `origin/main` was pushed through `7d63ebca`; isolated candidates and
  rejected speculative fixes remain separate from the integration branch.
- Make Meta+Shift left-drag consistently combine windows into QindaQt
  containers without competing KWin custom/thirds tiling. Normal and late-Shift
  grouping pass private runs `7bf2271d708648ee99e1afd80493ef40` and
  `d90a2e7ddd8747a38dea9e06dc106bb9`, including actual member-frame convergence
  and container-local Meta+Arrow. Standalone Welcome still tiles independently.
  These compositor repairs are included in the verified runtime installed for
  the QindaQt session started at 20:17 MDT.
- Show each window container as one dock/task-list entry, suppress its member
  entries, and route activation/minimize/restoration through the container.
  Detaching restores a standalone task; verify transfers, member exit, and
  normalization against the existing task-list contract.
- Remove competing default corner/edge tiling and make Alt-Tab visually fit
  QindaQt window containers. Private run `5f700906669c498ab4889da621667fc7`
  passes native QindaQt TabBox forward/reverse selection and activation with
  one container representative. Normal and late-Shift grouping runs above
  verify container-local versus standalone tiling. The installed compositor plugin matches the tested session; native visual
  acceptance is recorded separately from host capture, which was unavailable.
- Add a keyboard toggle for grouped-member title chrome, keeping container
  ownership and a visible focus cue in both clean-tile and title-visible modes.
- Provide a dismissible desktop shortcut note with an obvious way to reopen it;
  its actions must match the completed window-management behavior. Integrated
  `8ce25f98` provides persisted dismissal. Integrated `29be28fd` changes the
  default to Meta+Shift+F1 to avoid KWin Desktop 1. Private run
  `b752e061dc2a402f9729ab08b9606fca` verifies real hide/reopen input and the
  corrected visible label. The verified build is installed and the host
  binding is persisted as Meta+Shift+F1.
- Add QindaQt-specific parent-frame controls for member-title visibility and
  a compact management menu using the existing arrange, detach, and ungroup
  actions, without obscuring normal window controls.
- Make the active container and active member unmistakable through stronger
  theme-consistent frame cues, including when member titles are hidden.
- Replace stock KWin chrome context menus with QindaQt group/member-aware
  controls, exposing the same completed actions as the parent frame.
- Add whole-group roll-up/shade or iconify behavior appropriate to the selected
  interface, with parent controls and reliable member/focus restoration.
  Run `a1bdb644aa3f4cdebd6a174f5ad7d7d9` records the actual context-menu
  minimize action and taskbar restoration of both members. Its overall result
  remains false because display scaling failed; this is scoped container evidence.
- Provide an easy user-approved production mouse/keyboard path for agents and
  Gabbee, also usable for realistic testing and debugging. Reuse the standard
  RemoteDesktop portal and its approval/session lifetime instead of exposing
  the development-only injector. Verify pointer, keys, and text delivery into
  an explicitly chosen test application.
- Qualify Gabbee dictation, its global shortcuts, and text insertion into
  ordinary applications and terminals, including correct grouped-window focus.
  Integrated `f23f6710` has two real sink-to-PTY runs, each 11/11: standalone
  and grouped terminals receive separate unique Unicode sentinels, independently
  matched against shell-written files. The actual Gabbee checkout passes 187/187
  tests. Native push-to-talk run `bd01eade1a1442d7ac9bc4147ac293d2` and
  command run `29894db03d7d49d2ad9766a346b3f181` each verify exactly one
  press/release callback through the real KDE backend. Physical microphone
  transcription and user-specific F23/F24 activation are not claimed.
- Qualify fullscreen video and games: fullscreen entry/exit and restoration,
  input focus, panel visibility, and pointer capture/confinement behavior.
  Controlled native fullscreen run `0aca8a74926c44f98def9b87b2a83328` passes
  competing-peer rejection, outside Alt-Tab, and exact grouped restoration while
  retaining outside focus. Pointer lock/confinement/relative-motion run
  `d8f380054b0c44fe8b176692063e6e11` also passes. Both clean up completely;
  these representative clients do not establish compatibility with every game.

Claude, GLM through Kimi, Kimi, and Codex workers contribute isolated candidates
and independent reviews. Product completion requires integrated evidence;
worker activity alone does not advance the milestone. See [Handoff](HANDOFF.md)
for the current installed and integrated boundaries.

### Explicit checkpoint stopping rule (2026-09-06)

The renewed September 6 instruction freezes scope to finishing work already
started. Investigate the reported connectivity loss, close the existing
implementation and acceptance gaps, and deploy the accepted result. The
independent Kimi worker retains file-manager ownership. No new feature queue
is authorized by this completion request.

The user explicitly requested a goal with a finite stopping point. Finish the
reported desktop-experience list above as one usable deployed checkpoint:
independently reviewed integration, focused checks, one closing broad suite,
and a bounded actual-desktop pass for Settings, menu actions, grouping/chrome,
shortcuts, and fullscreen. Verify Gabbee with synthetic insertion and document
hardware-dependent evidence precisely. Refresh the tested shell/services and
perform the compositor session restart when its accepted changes are ready.
Stop when those outcomes pass; do not expand the checkpoint into unrelated
features or open-ended aesthetic polishing. A task is not complete merely
because its worker handed off code or a screenshot looks improved.

### Consistent appearance and first-launch tutorial (2026-09-05)

Correct light/dark and contrast across shell, popups, Settings, and first-party
applications. Add a detailed, attractive, human-readable startup tutorial with
a persistent **Show at next launch** checkbox and a launcher entry for reopening.
Contrast, shared live appearance, themed window chrome, and the seven-chapter
tutorial are integrated. The full build, eleven focused appearance/tutorial
checks, and installed desktop startup pass. Actual first-launch, compact layout,
live light/dark switching, tiling/tab interaction, and the display matrix are
verified. Notification control contrast is repaired and visually rechecked.
Host installation and QindaQt (Wayland) session registration are now complete.
The next bounded outcome is the first physical login; nested acceptance does
not claim full hardware daily-driver qualification.
See [Handoff](HANDOFF.md) for exact evidence and the remaining visual follow-up.

### QindaQt visual identity (2026-09-05)

Create a complete native icon theme and a coordinated original wallpaper set.
Mineral Light uses soft geometry, jade, ink, porcelain, and apricot. The icon
theme and five wallpapers are integrated, including the QindaPunk cyborg
penguin and Compile Club. Appearance selection changes the actual desktop
without restarting; existing user preferences remain authoritative. Exact
review, asset validation, runtime checks, and real desktop screenshots are
recorded in [Handoff](HANDOFF.md).

### Dock refinement and menu placement (2026-09-05)

Refine the QindaQt and macOS-inspired docks with content-sized translucent
surfaces, larger real icons, running indicators, spacing, and outward task
menus. Hide first-party in-window menus while the global menu actually hosts
their actions, restoring local menus when that host becomes unavailable.
Dock, panel, menu placement, and launcher Pin/Unpin changes are integrated.
Focused regression repairs and the profile/display matrix pass. The private
desktop verifies outward menus, global/local menu placement, transparent-margin
click-through, and real Pin/Unpin changes to Quick Launch. Launcher rows now
fill their viewport with readable names and compact actions. Evidence and
remaining refinement are recorded in [Handoff](HANDOFF.md).

### Audit repair acceptance (2026-09-05)

Repair the ten user-selected audit groups: outward Launcher/Bluetooth/Power
popups and Escape; real customization pointer drops; applied-content Discard
baselines; saved startup layout and merged user catalogs; shell appearance and
accessibility preferences; rendered global-menu action generations; child
configuration/authentication environment; panel zone budgets and real rows;
single-instance applet creation; and all 31 unresolved stock preset instances.
The selected repairs are integrated. Final acceptance includes exact candidate
review, integrated regression tests, and private nested visual interaction.
The shared chrome now combines tabs and group controls in one 28-pixel row
(right-to-left tabs in Mac style); native member title strips use 24 pixels.
Current evidence and remaining scope are recorded in [Handoff](HANDOFF.md).
This does not close the other audit findings or assert complete desktop-environment
parity. Memory optimization remains deferred behind usability.


### Shell and customization delivery queue

**User-stated acceptance (2026-09-04):** the deliverable is a full desktop environment that a person can use — styled panels, working applet
popups, window icons and application icons rather than text labels, a working terminal — verified by looking at a headless capture of the real
session, not only by the test suite. The production-shell runtime repair and the iconography outcome (QQ-004.16) are the active steps toward it.

Finish QQ-004 through the durable [Shell queue](../ops/team/queues/shell.md):
global menu; launcher, task list, tray, and remaining system applets; direct
WYSIWYG customization; and whole-shell output, DPI, theme, keyboard, and
accessibility qualification. Existing production panels and notification
qualification remain preserved integrated foundations.

### Platform services delivery queue

Finish QQ-005 through the durable [Platform queue](../ops/team/queues/platform.md):
remaining Display1 nested convergence and hardware work,
production Power/brightness upstream adapters,
Network credential/profile/radio mutation, persistence, external secret-agent
integration, and hardware qualification over the resident Network N1 boundary,
production BlueZ/UI over the Bluetooth B0 boundary, private clipboard history,
display color, font application,
host portal selection/toolkit reaction, and every non-Settings portal family.
The standard Settings appearance backend is now an executable integrated
foundation. Existing Audio1, Display1 foundations, and
resident Power PB-1 remain preserved integrated foundations; Network N1 now
owns the confined production NetworkManager transport and an installed public-
client-only Settings route, while Bluetooth B0 is an executable bounded
foundation without a production platform backend or UI.

### First-party experience delivery queue

Finish QQ-006 through the durable
[First-party queue](../ops/team/queues/first-party.md): complete Settings routes,
later File Manager and Terminal capabilities, application migrations, and
cross-app responsive, DPI, visual, keyboard, and accessibility qualification.
QST-1, Controls, AppShell, Text Editor, the read-only local File Manager S0,
the single-session Terminal S0, and live Notifications, Appearance, Display,
and Network Settings routes remain preserved integrated foundations.

### Interactive virtual desktop integration

The integrated QindaQt session must boot beneath an isolated parent Wayland
compositor, render the compositor, shell, resident platform services, and test
applications, accept synthetic input confined to its private nested seat, and
produce reviewable screenshots. Acceptance covers 1920x1080, 1920x1200 WUXGA,
and 2560x1440 with representative 100%, 125%, and 150% scale, light/dusk/dark
themes, and at least one multi-output arrangement. The workflow must prove that
it neither connects to nor moves the host desktop pointer.

The initial aggregate idle PSS ceiling is 1,024 MiB. A measured overage remains
a real defect, but optimization beyond that starting ceiling follows reliable
end-to-end boot, interaction, screenshot, teardown, and repeatability evidence.

## Completed outcomes

- 2026-09-05 — Added the separate [QindaQt handbook](wiki/handbook/index.md):
  17 categorized pages covering project philosophy, desktop behavior, applications,
  architecture, privacy, development, and complete snapshot catalogs of 7 features,
  34 steps, 174 evidence records, 48 settings, 10 profiles, 5 themes, 11 applet
  manifests, and 144 canonical wiki pages. Source snapshot `97286120`; documentation
  candidate `519ca7a6`, independently reviewed with no blocking findings.
  Strict MkDocs, repository links, catalog coverage, and all
  17 rendered pages' local destinations/anchors pass. This documentation outcome
  changes no product feature maturity or runtime qualification.

- 2026-09-05T02:40:44-06:00 — **Install readiness restored on exact main `3e658510`** (product commit of the polish merge). Integrated the desktop polish `511ac862` at `3e658510` (task icons proven in the nested capture, shell-owned/non-normal windows excluded from tasks, quiet empty chips, Do Not Disturb default); focused 326/326 in Debug and Release, static gates, broad safe Debug 634/634, nested boot/panel-visibility/interactive/CompositorShell1 rows 6/6, installed rows 14/14, session.installpaths 1/1, broad safe Release 634/634 with the /usr/local prefix confirmed. The manager inspected the headless interactive capture: icon chips, real application icons on task buttons, quiet global menu, styled notification center. The install/SDDM smoke requires the user's sudo.
- 2026-09-04T23:37:23-06:00 — **Exact main `6a2019aa` is verified and usable; install is allowed with three known visible polish defects** (task buttons show letter placeholders because the nested stage lacks desktop entries — the real prefix ships them; the shell's own popup is listed as a task; an empty chip renders as a white square) **that the open `desktop-polish` lane (Frances Arnold, OpenAI Codex) is fixing.** Integrated the icon-first panel applets `7eb5372d` at `6a2019aa` (Barbara McClintock ACCEPT `0/0/0/5` after one repair); focused 325/325 in Debug and Release, static gates, broad safe Debug 633/633, nested boot/panel-visibility/interactive/CompositorShell1 rows 6/6, installed rows 14/14, session.installpaths 1/1, broad safe Release 633/633 with the /usr/local prefix confirmed. Since the withdrawal: shell token publication (ADR-0071), iconography I1/I2 (ADR-0072), the atomic task-fact contract (ADR-0073), and the first-party menu export lifecycle are integrated; the manager inspected the headless interactive capture of this tree (icon chips, working task buttons, quiet global menu). The install/SDDM smoke requires the user's sudo.
- 2026-09-04T21:24:52-06:00 — Integrated the first-party global-menu export for Terminal and Text Editor with the fail-closed exporter lifecycle `bfe60099` at `42191636` (Nettie Stevens ACCEPT `0/0/0/0` after three Codex rejections closed successive lifecycle races); focused 190/190 in Debug and Release, static gates, broad safe Debug 630/630, serialized nested boot, panel-visibility and interactive rows 5/5 on the system KWin 6.6.6 roots. `Menu unavailable` no longer appears for first-party windows.
- 2026-09-04T20:37:02-06:00 — Integrated the compositor atomic task-fact contract `fa0e6d9c` at `92d3348b` (Rita Levi-Montalcini ACCEPT `0/0/0/1`; ADR-0073): the task list now lists and controls real windows instead of showing `Limited`; Release focused 202/202 and Debug broad safe 620/620 (which contains the focused rows), static gates, serialized nested boot and panel-visibility rows 4/4 on the system KWin 6.6.6 roots; the boot row now requires the task list ready with real windows.
- 2026-09-04T19:23:40-06:00 — Integrated the shell iconography module I1 `288574a8` at `1207bf39` (Carolyn Bertozzi ACCEPT `0/0/0/2` after one repair; ADR-0072); focused icons/shell-runtime/applet rows 14/14 in Debug and Release and the static gates pass; the broad suite runs with the icon-first applet batch. QQ-004.16 WIRED.
- 2026-09-04T18:42:24-06:00 — Integrated the production shell runtime repair `99de545a` at `cd760f4e` (Tu Youyou ACCEPT `0/0/0/4`): shell and preview publish the QST token facade before panel QML, the boot row validates a `tokens` fact, and the terminal PTY/render startup races are fixed; focused 198/198 rows in Debug and Release, static gates, broad safe Debug 615/615, serialized nested boot and panel-visibility rows 4/4 on the system KWin 6.6.6 roots. Still open before install readiness: the compositor task-fact contract (task list `Limited`), Terminal/Editor menu export (`Menu unavailable`), and the iconography pass; a headless interactive capture of this tree is being taken for visual review.
- 2026-09-04T15:32:08-06:00 — **Install readiness withdrawn.** A windowed run of the exact Release install tree on the host display (private buses, `qindaqt-wm --windowed`) shows an unusable session: the production shell never publishes the `QindaQt.Tokens` facade (only applications bootstrap it), so every QST-based hosted applet renders unstyled (~36k undefined-token warnings, white panels with bare labels); the task list shows `Limited` and the global menu `Menu unavailable` with an exporting terminal active; the terminal's content area stays blank although its bash children run. The green suite never asserted token readiness or live applet state in the production shell. Repair lane `shell-production-runtime-repair` (Ada Yonath, OpenAI Codex) is open from `dd415f48`; do not run the `/usr/local` install until its fix is integrated and a windowed run is visually verified.
- 2026-09-04T13:57:49-06:00 — Integrated the Color Settings route `85c8e8c` (assistant merge `0f2bf167`, landed `32b1ef71`), Task List T3 hosting `e78e407` (`fdd07126`, reconciliation `bdfef7da`), and Tray S3 hosting `a257d73` (assistant merge landed `4cafc37c`) after Maryna Viazovska's, Henrietta Leavitt's, and Jocelyn Bell Burnell's ACCEPT verdicts; on the system KWin 6.6.6 roots the merged tree `4cafc37c` passes 178/178 focused rows in Debug and Release, the static gates, the broad safe Debug suite 614/614, the serialized nested boot and panel-visibility rows 4/4, and the Release install boundary (installed rows 14/14, session.installpaths 1/1, broad safe Release 614/614, /usr/local prefix confirmed). QQ-006.05 widened (Color live at Ctrl+0); QQ-004.10 and QQ-004.11 now host the task list and tray in the production panels (ten built-ins rendered). Exact main `4cafc37c` is the install boundary.
- 2026-09-04T10:44:07-06:00 — Integrated Tray S2 `544d1c3` at merge `f3abd4ab` (reconciliations `5157a1e0`, `cbaab4e0`) after Rózsa Péter's recheck ACCEPT `0/0/0/0`, and Bluetooth pairing `e473bbf` at merge `4b3f07d9` after Kathrin Bringmann's recheck ACCEPT `0/0/0/0`; on the system KWin 6.6.6 the merged tree passes 105/105 focused Bluetooth/Settings/status-notifier/applet/shell-runtime rows in Debug and Release, the static gates, and the broad safe Debug suite 601/601. QQ-005.05 widened (Agent1 prompts, Pair/Remove/Trust, Settings/applet pairing UX); QQ-004.11 stays EXECUTABLE with the tray registered and its hosting lane open.
- 2026-09-03T13:55:54-06:00 — Qualified exact main `0998b1f4` for the real `/usr/local` install: Release builds cleanly with the host's system KWin 6.6.6, all 13 installed-label rows and `session.installpaths` pass, the broad safe Release suite passes 592/592, and strict documentation/source/JSON/Team Board gates are green. The remaining SDDM login smoke requires the user to run the documented sudo install and select “QindaQt (Wayland).”
- 2026-09-03T13:35:22-06:00 — Integrated Task List T2 `1e32f0a` at merge `431856a5` after Lauren Williams's terminal ACCEPT `0/0/0/0`; reconciliation `328f8b5e` corrected the merged dispatcher-count wording caught by the broad guard. On system KWin 6.6.6 the tree passes 33/33 focused rows in Debug and Release, strict static gates, every one of the 592 broad-safe Debug rows across the initial 591/592 sweep plus the corrected guard rerun, and all 8 serialized DesktopVirtual package/boot/loader/panel rows. QQ-004.10 now includes the registered compiled QST applet, but production hosting remains open; the named combined hosting lane was not opened because Tray S2 ended its only funded review at REJECT `0/1/0/0`.
- 2026-09-03T13:03:46-06:00 — Qualified the integrated KWin 6.6.6 panel-visibility repair `1d86b13` at merge `b5c8d87` after Dana Ulery's exact ACCEPT `0/0/0/1`: fresh manager verification on the host system KWin passes 125/125 focused rows in Debug and Release, static gates, broad safe Debug 586/586, and all 8 serialized DesktopVirtual package/boot/validator/loader/panel rows including `single-1080p` and `single-wuxga`, with no live KWin or Weston survivor. QQ-004.02 remains EXECUTABLE with its installed nested proof restored on the production compositor pin.
- 2026-09-03T12:47:31-06:00 — Integrated session actions `23d99f5` at merge `70763c7` (Session1 logout, lock/suspend/restart/shutdown client, Power applet and page sections, Meta+L, secret-agent autostart, ADR-0070); on the system KWin 6.6.6 the merged tree passes 61/61 focused rows in Debug and Release, the static gates, and the broad safe Debug suite 582/582; the nested boot row runs once the private seat frees. QQ-004.13 widened.
- 2026-09-03T12:30:19-06:00 — Integrated the Clipboard applet hosting `fef9222` at merge `82e256d` (reconciliation `dfe08e4` guards its QML rows behind the shell target) and the Network N3 visible-network join `beef29e` at merge `c269830`; on the system KWin 6.6.6 the merged tree passes 66/66 focused rows Debug/Release, broad safe Debug 579/579, static gates, and the nested boot row. QQ-004.15 and QQ-005.04 widened.
- 2026-09-03T11:21:44-06:00 — Switched the compositor pin to the host's system KWin 6.6.6 (`a5c1c20`, harness fixes for the sandbox library path and explicit `--kwin`, wayland-sessions entry) and integrated the first-party menu export `e37d906` (`a8c171c`, ADR-0068), the stage closure guard `91377ac` (`0a59236`), the Network secret agent `dbeab3f` (`287ba6d`, ADR-0069), and the Clipboard Settings route `6f4f728` (`22d9f23`, Ctrl+9). Against the system KWin: focused batch 118/118 Debug/Release, nested compositor rows 16/16, desktop boot, broad safe Debug 569/569, Release rows 88/88, static gates. Open: panel-visibility nested rows on 6.6.6.
- 2026-09-03T09:48:12-06:00 — Integrated tray S1 `68009bb` at merge `e3eacdd`; merged tree passes tray/shell-runtime/applet rows 16/16 Debug/Release, broad safe Debug 551/551, and static gates. QQ-004.11 EXECUTABLE.
- 2026-09-03T09:40:00-06:00 — Integrated Task list T1 `bf555ed` at merge `90fe400`; merged tree passes task-list/shell-runtime/applet rows 22/22 Debug/Release, broad safe Debug 547/547, and static gates. QQ-004.10 EXECUTABLE.
- 2026-09-03T09:33:11-06:00 — Integrated the Clipboard applet C1 `72a79fd` (assistant merge `35a548f`, landed `2abc448`), Font F1 `4f5452f` (assistant merge `8d4c880`, landed `045f1a1`), and the panel-visibility interaction repair `cae66fc` (`ff08ea2`) after the staging reconciliations `e51372a`/`197f104` and the source-shape split `fb2b280`; reconciled tree passes 153/153 focused rows Debug/Release, broad safe Debug 543/543, static gates, and the nested desktop boot and both panel-visibility rows serially. QQ-004.15 EXECUTABLE, QQ-005.08 widened, QQ-004.02 EXECUTABLE.
- 2026-09-03T07:15:39-06:00 — Integrated the Power Settings route `0392aee` at merge `a83f34e` (reconciliation `3afa972` links the static PowerBackend module into the Main.qml test hosts) and Display Color C1 `4c4f2c4` at merge `f6b14b2` (ADR renumbered 0066); merged tree passes Settings/power/display-color rows 89/89 Debug/Release, broad safe Debug 520/520, and static gates. QQ-006.05 widened (Power at Ctrl+8); QQ-005.07 widened.
- 2026-09-03T06:33:14-06:00 — Integrated Text Editor S2 `d68f8b1` at merge `28bcd37` (ADR renumbered 0065); merged tree passes editor/settings/app-shell/terminal/file-manager rows 97/97 Debug/Release, static gates, and broad safe Debug 494/495 (the one failing `desktop.virtual.interaction-probe-cli-unit` row passes on isolated re-run while a nested-row review runs concurrently). QQ-006.06 widened.
- 2026-09-03T06:07:45-06:00 — Integrated Terminal S2 `e030a42` at merge `9e3422e`; merged tree passes terminal/app-shell/editor rows 34/34 Debug/Release, broad safe Debug 489/489, and static gates. QQ-006.08 widened (search and links).
- 2026-09-03T05:37:29-06:00 — Integrated Global Menu G2 `24240e9` at merge `7de332f` and File Manager S1 `9ade95a` at merge `0b97954`, with two manager reconciliations (`a1bac21` launcher guard expects seven dispatcher entries; `4609beb` adds the omitted `QindaQt.Shell.GlobalMenu` test stub that had broken the launcher dispatcher and notification-center offscreen rows); the reconciled tree passes 87/87 focused rows Debug/Release, broad safe Debug 485/485, and static gates. QQ-004.06 and QQ-006.07 widened.
- 2026-09-03T04:58:12-06:00 — Integrated the Bluetooth Settings route `24129a2` at merge `111dedb` (delegated registry reconciliation, landed at `500a33e`); merged tree passes Settings and Bluetooth rows 64/64 in Debug and Release under host-bus isolation, broad safe Debug 472/472, and static gates. QQ-006.05 widened (Bluetooth route live at Ctrl+7).
- 2026-09-03T04:42:48-06:00 — Integrated the Controls visual gate host-independence candidate `b7b5208` at merge `5cf24a2` (owned paths only; the branch's stale synced files dropped); merged tree passes controls 34/34 Debug/Release, visual rows twice, broad safe Debug 465/465 now including the 25 visual rows, and static gates. QQ-006.02 stays QUALIFIED with pinned fixtures.
- 2026-09-03T04:32:48-06:00 — Integrated the Audio Settings route `d10abe2` at merge `f6d47db` (delegated registry reconciliation by an integration assistant, landed at `fac8d6a`); merged tree passes all Settings rows (audio 4, customize 7, center/package/navigation rows) in Debug and Release under host-bus isolation, broad safe Debug 435 rows with zero failures, and static gates. QQ-006.05 widened (Audio route live); Bluetooth Settings `24129a2` accepted 0/0/0/0 and its merge delegated next.
- `8505bdb` and `7ecdb36` — The compositor's authenticated `CompositorShell1` boundary now projects a revisioned
  active-window identity (credentials-derived client PID or XWayland client id, AppMenu window id, announced
  app-menu service/path) with an exported change signal validated by the public generation rule on both sides
  (ADR-0063), and the compiled launcher applet is hosted in the production shell panels through a composition
  with production seams and a closed install component. Margaret Rock (OpenAI Codex) rejected the identity
  candidate once at `0/2/1/0` and accepted the repair at `0/0/0/0`; Annie Cannon (Z.AI GLM) accepted the hosting
  at `0/0/0/2`. The fresh merged tree passes compositor 36/36 non-nested rows, the window-actions/identity and
  client rows, the nested KWin rows serially, launcher 17/17, and the integrity/runtime/closure/installed rows in
  Debug and Release, the broad safe Debug suite passes 430/430, and all static gates. QQ-004.07 advances WIRED → EXECUTABLE; the Global Menu G2
  composition is unblocked.

- `00f2db9` — Terminal S1 adds a bounded multi-session tab strip in which every session owns its PTY/child with
  process-group-complete teardown, validated profiles within the existing launch policy, Settings1
  persistence with presented asynchronous apply outcomes, AppShell action-catalog exposure, exact argv
  preservation, and keyboard/accessibility parity. Dina St Johnston (OpenAI Codex, a different worker)
  rejected the first candidate at `0/2/1/1` for a shutdown that reported clean while an HUP-immune descendant
  survived and for apply failures never presented, rejected the next at `0/0/1/0` for a vacuous dialog
  proof, and accepted the second repair at `0/0/0/0`. The fresh merged tree passes terminal 15/15 in Debug and
  Release under host-unset isolation, the broad safe Debug suite passes 427/427, and all static gates. QQ-006.08 stays EXECUTABLE with a wider
  stopping point; search, links, global-menu export, and the nested matrix remain.

- `26f366a` — Launcher L1 adds the production adapters around the pure L0 model: an injected-root desktop-entry
  scanner with canonical containment, non-regular-file refusal, capped reads, debounced generation-fenced
  refresh, and errno-aware degraded truth; Settings1 pinned/recent persistence with uncertainty convergence; a
  seam-based bounded execution adapter with entry-level policy inheritance and no shell interpolation; and a
  compiled, registered, fatal-warning-clean launcher applet with persistence status, keyboard traversal, and
  accessibility (ADR-0062). Kay McNulty (OpenAI Codex, a different worker) rejected three candidates,
  including a host-isolation P0, before accepting the third repair at `0/0/0/0`. The fresh merged tree passes
  launcher 15/15 and applet integrity rows in Debug and Release under host-unset isolation, the broad safe Debug suite passes 421/421, and all
  static gates. QQ-004.07 stays WIRED with a wider stopping point until the production dispatcher hosts the applet.

- `2500a3d` — The direct Customize Settings canvas composes the integrated customization-editor domain into an
  installed `qindaqt-settings --page customize` route: a scaled WYSIWYG canvas with a manifest-sourced applet
  palette and properties pane, pointer drag through the domain's preview bracket with exactly one undo step
  per gesture, complete keyboard parity with accessible identities, Settings1 profile selection, atomic
  user-profile persistence, and dirty-draft discard confirmation on route departure and window close across
  responsive host switches. Klara Dan von Neumann (Kimi K3) rejected the first candidate at `0/0/2/3`; Adele
  Goldstine (OpenAI Codex) rejected two descendants at `0/0/2/0` and `0/0/1/0` and accepted the third repair
  at `0/0/0/0`. The fresh merged tree passes customize 17/17 (route plus domain rows) and Settings Center 9/9 in Debug and Release,
  the broad safe Debug suite passes 412/412, and all static gates. This advances QQ-004.08 from WIRED to EXECUTABLE; live-shell binding, reveal
  affordances, and the nested matrix remain.

- `3690a05` — The compositor exposes an authenticated `CompositorShell1` window-action boundary (activate,
  minimize, unminimize, close, raise) admitted only for the D-Bus caller whose unix credentials match the
  bound panel-owning shell client, with authentication before any parsing, explicit entry bounds,
  constant-size unauthenticated replies, generation fencing, rate limits, Hybrid container policy routing,
  and an exact-owner asynchronous shell client (ADR-0061). Margaret Rock (OpenAI Codex, a different
  worker) rejected the first candidate at `0/1/1/0` for pre-authentication parsing and unbounded reflected
  replies and accepted the repair at `0/0/0/0`. The fresh merged tree passes compositor 35/35 non-nested
  rows and the four window-action rows including the nested live row in Debug and Release, the sixteen `compositor.kwin-` rows pass 16/16 serially,
  the broad safe Debug suite passes 406/406, and all static gates. This widens QQ-004.10 without changing its WIRED state; the shell-side
  facts producer and applet hosting remain.

- `6cef8b5` — Power PB-2 adds the production upstream adapters behind PB-1's collaborator seams: UPower with
  correct line-power/PowerSupply/battery semantics, logind actions re-authorized at dispatch time with
  generation-fenced exactly-once completion across restart, power-profiles, and an injected-root backlight
  adapter, all selected by the composition root in production with an explicit deterministic mode
  (ADR-0060). Ida Holz (OpenAI Codex, a different worker) rejected the first two candidates at `0/2/1/1`
  and `0/1/0/0` and accepted the second repair at `0/0/0/0`. The fresh merged tree passes the power selector
  26/26 in Debug and Release under an unreachable host system bus, the broad safe Debug suite passes 403/403, and all static gates. QQ-005.03
  stays EXECUTABLE with production adapters; physical hardware, suspend/resume, and the Settings page remain.

- `14f3e67` — The production Audio applet composes only the public AudioClient through a shell-private
  controller with separate read and control grants, audited manifest/registry/host/profile routing, compiled
  keyboard-accessible QML, and exact-owner replacement fences. Integration exposed that the shell's new
  Controls/Tokens link dependency was not staged by the Power, Bluetooth, or default install components;
  the accepted staging-closure descendant ships that closure in every shell-carrying component and adds a
  component-closure row. Reviews: `caaf7d9` accepted `0/0/0/0` on Codex, `b623b00` rejected `0/1/0/0`,
  `14f3e67` accepted `0/0/0/0` by Frances Bilas (OpenAI Codex). The fresh merged tree passes applet 22/22 and
  integrity/runtime/closure 7/7 in Debug and Release, the broad safe Debug suite passes 396/396, and all static gates. This advances
  QQ-004.12 from WIRED to EXECUTABLE; physical devices and nested panel interaction remain.

- `c33b490` — Portal P1 proves host frontend selection and toolkit reaction with the real `xdg-desktop-portal`
  1.20.4 on a private bus: the QindaQt Settings backend is selected only under `XDG_CURRENT_DESKTOP=qindaqt`,
  the frontend's values and `SettingChanged` follow the QindaQt projection, Qt's `xdgdesktopportal` platform
  theme reacts live, and every non-Settings family routes through an explicit fail-closed fallback table
  (ADR-0059). Gertrude Blanch (OpenAI Codex, a different worker) accepted it at P0/P1/P2/P3 `0/0/0/0` with
  9/9 in Debug and Release; the fresh merged tree repeats 9/9, the broad safe Debug suite passes 391/391, and all static gates. QQ-005.09
  stays EXECUTABLE with host selection and Qt reaction now proven; GTK/Flatpak reaction and the other
  portal families remain.

- `63e884c` — The Clipboard C1 service adds a bounded `ext-data-control-v1` capture adapter, the private-bus
  `org.qindaqt.Clipboard1` protocol and exact-owner client with bounded remembered-request eviction, and a
  resident host that captures only after an explicit user-override opt-in (the Settings1 schema default is
  now `false`), withdraws truth unless the authenticated lock state is Unlocked, and ships activation
  artifacts. Evelyn Berezin (Kimi K3-256k) rejected the first candidate at `0/1/1/3` for first-start capture
  without consent and permanent request-cache exhaustion; Ruth Lichterman (OpenAI Codex) accepted the
  repair at `0/0/0/0`. The fresh merged tree passes clipboard 14/14 and Settings-related 29/29 in Debug and
  Release, the broad safe Debug suite passes 389/389, and all static gates; the candidate's ADR is renumbered to 0058. This advances
  QQ-005.06 from WIRED to EXECUTABLE; applet composition, live nested capture, and persistence remain.

- `f44919a` — The production BlueZ adapter reaches `org.bluez` through an injected direct-QtDBus
  connection behind the accepted AdapterBackend port, driving only adapter power, the reference-counted
  discovery lease, and paired-device connect/disconnect while BlueZ keeps pairing and trust authority;
  owner loss retires truth, hostile properties are bounded, and the composition root defaults to production
  with an explicit deterministic mode. Betty Holberton's independent Kimi K3 exact review accepted it at
  P0/P1/P2/P3 `0/0/0/2` with 14/14 rows in Debug and Release under an unreachable host system bus. The fresh
  merged tree passes 15/15 Bluetooth rows including the whole-repository staged-install row in both profiles,
  the broad safe Debug suite 379/379, and all static gates; the candidate's ADR is renumbered to 0057 to
  follow the already integrated ADR-0056. QQ-005.05 stays EXECUTABLE with a production backend; physical
  radios, pairing UX, Settings UI, and hardware qualification remain.

- `882cc0c` — The production Bluetooth applet B1 composes only the public Bluetooth client
  through a shell-private controller with separate read and control grants, audited
  manifest/registry/host/profile routing, compiled keyboard-accessible QML, and exact-owner
  replacement fences. Its controller surface is now proven by a compiled QMetaObject contract
  with token-paste, public-slot, and enum negative controls instead of the former regex gate,
  while textual composition-chain and dependency-policy contracts keep their independent
  poisons. After a 2/2 split on the regex ancestor, the same reviewers Cecilia Payne (Kimi K2.7)
  and Chien-Shiung Wu (Kimi K3-256k) accepted the repair descendant at P0/P1/P2/P3 `0/0/0/0`.
  The fresh merged tree passes Bluetooth 8/8 and adjacent 6/6 in Debug and Release, direct
  boundary gates 7+6 and 5+4, the broad safe Debug suite 373/373 (nested-compositor and host-font visual rows excluded), and all static gates. This advances
  QQ-004.14 from ABSENT to EXECUTABLE; the B0 service still runs the deterministic unavailable
  backend, so BlueZ, pairing UX, nested interaction, and hardware remain later outcomes.

- `7c27ee5` — Global Menu G1 adds the production transports behind the G0 foundation: an exact-owner
  `com.canonical.AppMenu.Registrar` service on an injected bus keyed to caller unique names with owner-loss
  retirement, an asynchronous `com.canonical.dbusmenu` client whose decoder bounds depth, counts, and lengths and
  rejects hostile or stale layouts atomically, and a transport coordinator bound to the proof-bound G0 lineage.
  Elizabeth Feinler's independent Kimi K3 exact review accepted it at P0/P1/P2/P3 `0/0/0/1` after reproducing
  Debug and Release 16/16 and staging hostile second-owner, decoder, and stale-revision reproductions. The
  fresh merged tree repeats 16/16 in both profiles; docs, strict MkDocs, shape, diff, and Team Board gates pass.
  Shell runtime instantiation, applet wiring, submenu popups, and installed-session proof remain later work, so
  QQ-004.06 stays EXECUTABLE with a wider stopping point.

- `4d4b3dc` — The private S3 desktop executes the production compositor,
  shell, resident services, Settings, and Text Editor across WUXGA, 1440p at
  125%, 1080p at 150%, light/dusk/dark themes, and a dual-output arrangement.
  Exact shell-readiness joins prove the active GlobalAccel component/action,
  stable unique shell owner and PID, closed/hidden center before the sole
  private-seat Meta+N batch, open/visible center with an increased counter
  after it, and one mapped compositor surface on the selected output. Mina
  Shah's external Claude source/archive review accepted the immutable
  candidate at P0/P1/P2/P3 `0/0/0/2`; Lise Meitner's independent fresh
  static+dynamic review accepted it at `0/0/0/0` after an 882/882 build,
  focused 5/5, formerly failing 1080p@150% 2/2, and package-plus-matrix 5/5.
  Four fresh archives prove containment 12/12, bounded PSS, nontrivial
  captures, empty teardown, and exact dual `[WL-1, WL-0]` authority with
  interaction and capture on WL-1. Fresh merged-tree replay also builds
  882/882, passes focused 5/5, formerly failing 1080p@150% plus package 2/2,
  and an unretried package-plus-four-row matrix 5/5; its four captures are
  byte-authenticated and visually coherent, all PSS values remain below the
  1,024 MiB ceiling, and final process inspection is empty. This advances
  QQ-004.09 and QQ-006.09 to
  EXECUTABLE without claiming complete screen-reader/keyboard coverage,
  heterogeneous mixed scaling, physical devices, GPU/DRM, hotplug, or
  perceptual baseline qualification.

- `6f5d0ba9` — The installed Network Settings route composes only the public
  Network1 client into bounded, secret-free inventory and admitted saved-
  profile actions. Barbara Liskov's external Claude exact rereview accepted
  the repair at P0/P1/P2/P3 `0/0/0/3` after directly closing all four former
  P2 findings. Fresh manager Debug and Release each build 2,036/2,036 and pass
  mutation 5/5, affected 14/14, public package/policy 5/5, Network 25/25, and
  Settings 9/9. Credentials, secret-agent ownership, profile editing, radio
  mutation, persistence, session-runtime proof, and physical qualification
  remain separate outcomes, so QQ-006.05 remains WIRED.

- `9f59a77a` — The standard Settings v1 portal backend exports confirmed
  QindaQt Settings1/QST appearance through the standard desktop-portal
  endpoint, with exact owner/epoch withdrawal, bounded standard values,
  activation, hardened service packaging, and complete Settings-only source
  and staged metadata. Frances Allen's exact rereview accepted the repaired
  descendant at P0/P1/P2/P3 `0/0/0/0` after independently proving the former
  installed Background escape is closed. Debug and Release review and fresh
  manager gates each build 97/97 and pass the contained 7/7 selector; docs,
  strict MkDocs, shape, package, provenance, and residue gates pass. This
  advances QQ-005.09 from ABSENT to EXECUTABLE without claiming host frontend
  selection, toolkit reaction, or any non-Settings portal family.

- `89557a0a` — Notification popup and center output selection now consumes the
  exact-owner ordered public compositor-output authority and accepts it only
  when generation and complete output-ID sets match accepted shell visibility
  and Qt inventory. Missing, stale, malformed, replaced, or unmatched truth
  clears both roles instead of falling back to Qt's stale primary screen.
  Charles Babbage's independent review accepted the immutable repair with
  P0/P1/P2/P3 `0/0/0/1`; fresh manager-tree Debug and Release each build
  458/458 bounded actions and pass the complete 20/20 adjacent selector. The
  remaining P3 prose precision is corrected here. The real dual-output
  layer-surface transfer and broader whole-shell matrix remain S3 work, so
  QQ-004.09 stays WIRED.

- `9a7872ae` — Display D6 packages the authenticated resident composition of
  D2 service, D4 public writer, D5 durable journal, compositor peer, lock, and
  logind authorities. Typed observation disposition preserves live truth and
  active transactions across benign same-owner rejection while failed owner
  replacement still withdraws stale authority. Mary Jackson's exact rereview
  accepted the repaired descendant after both former P1 defects closed.
  Fresh manager Debug/Release targeted builds complete 197/197 and 228/228;
  both pass hostile service 19/19, focused D6 7/7, and adjacent
  D0-D6/session-lock 40/40. Nested convergence, mixed/physical
  outputs, resources, suspend/hotplug, and hardware qualification remain.

- `fa22af50` — GCC 15.3 strict Release portability repair for the
  customization-editor panel-step value. The accepted two-path change
  aggregate-initializes the exact panel/zone/null-anchor tuple and avoids the
  diagnosed nested-optional inactive-storage move without suppression or
  semantic drift. Grace Hopper's exact review found P0/P1/P2/P3 `0/0/0/0`,
  reproduced the parent failure at action 59/85, and passed strict Debug and
  Release 85/85 builds, the full selector 6/6, and the direct repaired row 3/3
  in each profile. QQ-004.08 remains WIRED pending its Settings canvas,
  live-shell binding, reveal presentation, rendered matrix, and installed
  session outcomes.

- `aebc4fd3` — Resident Network N1 ownership, public Qt transport, confined
  libnm NetworkManager adapter, activation/package lifecycle, exact upstream
  owner-generation retirement, queued delayed-reply dispatch, and conservative
  scan-lease truth. Katherine Johnson's independent rereview passed with
  P0/P1/P2/P3 `0/0/0/0`, strict Debug and Release 21/21 each, 40/40 repeated
  mutation-sensitive executions, and boundary/six-poison/installed lifecycle
  3/3. Fresh manager Debug and Release each build 111/111 focused actions and
  pass the complete 21/21 Network selector. UI, persistence, external
  secret-agent/credential interaction, physical radios, distribution policy,
  and hardware qualification remain later.

- `9e4b7a60` — Production Power applet composition over the public PB-1 client,
  with separate read/control grants, exact-owner and pending-operation fences,
  audited manifest/registry/host/profile routing, compiled keyboard-accessible
  QML, and installed relocation/source-poison proof. Ada Lovelace's exact
  descendant rereview passed with P0/P1/P2/P3 `0/0/0/0`; fresh manager-tree
  Debug and Release each build 327/327 focused actions and pass 14/14 combined
  applet/host/Power rows plus 11/11 direct manifest cases. Live upstream
  providers, successful host mutations, nested interaction, and physical
  hardware remain later outcomes.

- `2f429c11` via manager merge `7277771a` — First-class Display Settings route
  over the public D3 client/coordinator with bounded draft/topology validation,
  preview/confirm/revert, authoritative coordinate refresh, keyboard-accessible
  output selection and integer coordinate commits, and installed route/package
  composition. The exact docs-only descendant passed terminal independent
  rereview P0/P1/P2/P3 `0/0/0/0`; its fully exercised parent passed strict
  Debug/Release builds 328/328 each, focused 12/12 each, compiled page 10/10
  each, interaction probe 7/7, and four mutation controls. Fresh merged-tree
  verification repeats 328/328 and 12/12. Resident writer composition, nested
  convergence, physical displays, and live assistive technology remain later.

- `acd0168` — Display D5 adds a separate crash-safe filesystem journal adapter
  with an injected, ownership-validated state root; fixed bounded paths;
  canonical hostile-input decoding; mode-0600 exclusive temporary writes;
  file and directory durability barriers; atomic replacement; safe load and
  clear behavior; and typed post-commit durability uncertainty propagated
  through D4 into D1. An already-absent clear retries the directory barrier,
  and composed recovery stays cleanup-only `Stuck` with zero compositor apply
  requests until durable absence is proven. Independent exact rereview passed
  P0/P1/P2/P3 `0/0/0/0`, strict Debug and Release 12/12, direct lifecycle 4/4,
  adjacent D2/D3 10/10, package, docs, shape, lineage, provenance, and residue
  gates; manager replay built 130/130 focused actions and passed 12/12 plus
  adjacent client 5/5. D6 now supplies resident startup recovery/writer
  composition and authenticated lock/logind safety; nested convergence, mixed
  outputs, resource proof, and hardware qualification remain later outcomes.

- `a8a57a9` — Resident Power PB-1 service/client, exact-owner asynchronous
  transport, installed package, private activation/residency lifecycle, and
  fail-closed multi-domain publication are integrated. Independent exact
  rereview passed P0/P1/P2/P3 `0/0/0/0`, Debug and Release selectors 8/8,
  seven hostile mutation paths, and the collision-clear, battery-disappearance
  and malformed non-resurrection probes. Production UPower/logind/profile/
  brightness adapters, Settings/shell UI, persistence, policy, hardware and
  suspend/hotplug qualification remain later outcomes.

- `26bb7f5` — Private interactive desktop S2 boots the production compositor,
  shell, resident services, Settings, and Text Editor beneath an isolated
  Weston parent at 1920x1080; injects exact `Meta+N` through the private nested
  seat; observes a 440x640 active notification center; captures and validates
  its exact framebuffer region; accounts for all eight QindaQt production
  roles below the 1,024 MiB ceiling; and tears down with zero authenticated
  survivors. Independent exact review passed P0/P1/P2/P3 `0/0/0/0`, a fresh
  2,338-action build, 73/73 units, and both private boot/interaction rows.
  WUXGA, 1440p, fractional scales, theme variants, multi-output, broader
  accessibility, and physical-device proof remain the next matrix outcome.

- `d7691ac` — Display D4 adds a bounded public QtWayland KDE
  output-management writer with complete/surviving-value mapping, exact
  owner/lineage/request fencing, synchronous-callback deferral, restart and
  proxy-lifetime safety, pinned protocol inputs, and an installed poison-tested
  boundary. Independent review passed `0/0/0/0`; fresh integrated-tree Debug
  verification built all 23 executable Display targets and passed D0-D4
  26/26. Resident writer/journal composition, authenticated lock/logind policy,
  nested convergence, and hardware remain later.

- `c819db8` — Typed asynchronous Display1 client and reversible transaction
  coordinator with exact-owner activation, validated atomic snapshots,
  owner/epoch/revision and late-reply fencing, bounded operation completion,
  installed public/private package proof, and a real private-bus lifecycle.
  The current-manager replay passed exact Gemini review `0/0/0/0`; fresh
  strict Debug and Release manager builds each completed 81/81 targeted
  actions and passed the seven-row D2/D3 selector. The separately integrated
  D4 writer and D5 journal now supply the public mutation and durability
  boundaries, and the Display Settings route is integrated separately;
  resident composition, nested convergence, hardware, and resource
  qualification remain later outcomes.
- `0c9f4b0` — Native Settings Center S1 with a typed bounded route registry,
  stable per-route lifetime, responsive wide/compact navigation, guarded
  unavailable-route focus, keyboard and accessibility paths, sanitized
  installed packaging, and ADR-0048 route ownership. The repaired descendant
  passed exact independent review `0/0/0/0`; Debug/Release passed 9/9, the
  direct fatal-warning page test passed 6/6, the external navigation harness
  passed 5/5, package-isolation poison passed, and fresh manager-tree gates
  passed. Remaining platform pages, drag-from-configuration customization,
  cross-app visual matrices, and live assistive-technology proof remain later.
- `2ae29f3` — Display Color C0 pure model with strict bounded ICC header and
  catalog validation, deterministic capability-aware assignment, canonical
  lineage fingerprinting, and atomic revisioned snapshots. The exact GLM
  repair passed independent Gemini Pro review with `0/0/0/0`; all eight hostile
  reproductions are defeated, strict Debug/Release builds pass 6/6 registered
  rows and 46/46 direct cases, and fresh manager-tree gates pass. Live profile
  discovery/import, persistence, compositor application, Settings UI, nested
  HDR/WCG evidence, and physical hardware remain later slices.
- `ea4d986` — Network N0 bounded protocol, canonical identity/codec/redaction,
  pure lineage/lease/intent model, and injected exact-owner asynchronous client.
  Exact independent replay review passed `0/0/0/0`, Debug/Release 13/13,
  direct 118/118, all eight mutation controls, package poison, 49/49 leaf-byte
  equality, and seven additions-only shared registries. The combined manager
  tree passes 64/64 focused build actions, 13/13 rows, source shape, 99-page
  docs, and strict MkDocs. Resident service, NetworkManager/secret transport,
  persistence, UI, radio mutation, and hardware qualification remain N1+.
- `c08b32e` — Bluetooth B0 bounded protocol/model/client/resident service with
  exact unique-owner lineage, deterministic least-authority backend, bounded
  discovery leases, activation and owner-loss lifecycle, configured D-Bus and
  systemd packaging, and BlueZ-owned pairing/trust authority. Exact independent
  replay review passed `0/0/0/0`, 9/9 including staged install, 70/70 direct,
  package poison, and 54/54 blob identity. The manager tree passes all eight
  source/private-bus rows plus source shape, 96-page docs, strict MkDocs, and
  Team Board 17/17. Production BlueZ, physical adapters/rfkill, Agent1 UX,
  Bluetooth audio, suspend/hotplug, UI, and hardware proof remain later.
- `4f99a7f` — Native QindaQt Terminal S0 with an owned child PTY, nonblocking
  teletype bridge, bounded launch policy, deterministic child/PTY teardown,
  QindaQt theme projection, truthful selection/copy behavior, qtermwidget
  confined behind one adapter, desktop metadata, and relocatable installed
  packaging. Exact independent review passed with zero findings; the manager
  tree passes 63/63 build actions, 9/9 registered rows, 7/7 appearance and 4/4
  real-adapter cases, source shape, 93-page docs, and strict MkDocs. Multiple
  tabs/profiles, settings persistence, AppShell/global-menu migration, whole-
  application accessibility, and the nested screenshot matrix remain later.
- `d0e0809` — The native QindaQt Text Editor now consumes
  `QindaQt.AppShell 1.0` through a typed action catalog, fail-closed file
  selection request/result bridge, lifecycle/integration projection, and
  consumer-owned native picker adapter. Exact independent Debug/Release,
  hostile coordinator, component-only package/RPATH, source-policy, adjacent
  application, documentation, and manager-tree verification pass. This does
  not claim a real portal transport or global-menu exporter.
- `d71fac4` — First-party Appearance Settings S0 as an ordinary
  `qindaqt-settings --page appearance` route with validated theme, scheme,
  font, smoothing, wallpaper, and logical-scale intent; per-key Settings1
  draft/apply/conflict/no-replay truth; complete QST preview; compact
  keyboard/accessibility traversal; and sanitized installed-route packaging.
  Applying those preferences to the compositor, displays, fonts, wallpaper,
  and other applications remains with later platform and convergence slices.
- `3fd3842` — Native QindaQt File Manager S0 with bounded local-directory
  launch intent, asynchronous listing, navigation history, QST/Controls UI,
  keyboard/accessibility metadata, desktop packaging, and a relocatable
  component-only installed runtime. The slice is deliberately read-only;
  mutation, mounts, trash, search, previews, portals, recovery, nested visual
  matrices, and live assistive-technology qualification remain later work.
- `d08747d` — Contained virtual-desktop S0+S1 source boundary: authenticated
  bubblewrap sandbox and staging, exact production package contract, bounded
  simultaneous topology/readiness proof, application/output/input/dock
  identity checks, aggregate 1,024 MiB PSS accounting, authenticated teardown,
  and failure-safe evidence archival. The registered private 1080p boot row is
  not yet qualified and contributes no live-desktop or screenshot claim.
- `3078386` — PB-0 bounded Power1 values/codecs, deterministic aggregate-
  battery policy, result lineage, and pure brightness composition/math with
  fail-closed identity, mirror collapse, integer conversion, installed public
  headers, and focused tests. PB-1 now composes this foundation into the
  resident executable boundary recorded at `a8a57a9`.
- `5c914a6` — Narrow installed `QindaQt.AppShell 1.0` shared boundary with
  atomic action/menu values, lifecycle and injected integration state,
  fail-closed portal replies, close consent, focus reporting, truthful
  degraded/unavailable presentation, accessibility identity, and an installed
  consumer. Real portal adapters, application migrations, and nested/live-AT
  qualification remain later outcomes.
- `a5528f8` — Resident `org.qindaqt.Display1` service and exact-owner
  compositor inventory adapter with restart-unique process lineage, hostile
  A/B/A epoch-reuse rejection, private-D-Bus owner replacement, deadline
  re-arm, and complete observer/name/object teardown. Output mutation remains
  fail-closed pending durable journal/resident writer composition and UI; the
  typed client is integrated separately at `c819db8` and the bounded writer at
  `d7691ac`.
- `1b4e284` — Installed live notification interaction qualification: real
  `Meta+N` registration/remapping, keyboard/focus traversal, Settings1
  persistence/failure/replacement, Do Not Disturb and critical bypass, shell
  restart, authenticated private lock privacy, teardown, the complete
  1080p/WUXGA/1440p/125%/150% matrix, and ten repeated 1080p lifecycles.
- `1cd5dab` — Native QindaQt Text Editor S1 with one local UTF-8 document,
  optimistic conflict detection, atomic persistence, QST/Controls presentation,
  keyboard and accessibility metadata, installed packaging, and bounded
  large-document behavior.
- `fac2756` — Bounded `org.qindaqt.Audio1` protocol, asynchronous Qt client,
  resident service, confined WirePlumber worker, deterministic reset
  lifecycle, and isolated null-device runtime and package qualification.
- `05a8636` — QST-1 semantic design tokens, immutable palette/metric
  derivation, accessibility overrides, read-only QML exposure, and installed
  C++/QML consumer boundaries.
- `c498269` — Persistent notification quieting through generic Settings1,
  including the Notifications settings route, shell projection, restart and
  transport-loss behavior, and independent staged-service qualification.
- `11c1f4b` — Notification presentation is denied unless an authenticated
  compositor-bound lock monitor conclusively reports `Unlocked`; transport
  loss revokes visibility immediately.
- `c93c45e` — Session-scoped Do Not Disturb policy and presentation behavior.
- Hybrid interaction, Compositor MVP, and Foundation are complete as recorded
  in the implementation roadmap.

## Later outcomes

- Production Power and brightness adapters, policy, UI, persistence and
  hardware work from the accepted PB-0…PB-5 architecture; Bluetooth, network,
  clipboard, remaining display, color, font,
  portal, and policy platform services.
- The complete applet-based settings center and remaining first-party desktop
  experiences.
- Physical hardware, performance/memory, packaging, recovery, migration, and
  upgrade qualification.

### Audit repair priority clarification (2026-09-05)

The user explicitly prioritizes a working, intuitive desktop and coherent
appearance over additional security architecture. Audit repairs must not add
permission provisioning, sandbox infrastructure, or capability abstractions
solely for hardening. Simplify gates that unnecessarily disable ordinary desktop
controls; retain state correctness, persistence, recovery, and intentional
confirmation of session-ending actions. Update normative architecture where a
concrete simplification changes its contract. Show actual private desktop
screenshots in the conversation as visual verification proceeds.

The 1,024 MiB aggregate idle PSS target is explicitly deferred by the user.
Memory measurements remain useful evidence, but this audit repair effort does
not gate functional completion on that target or spend implementation time on
optimization. Working desktop behavior and usability come first.

### Compact container chrome follow-up

The user requests space-efficient window chrome: tabs inside the main title
bar, right-to-left placement in the macOS layout, and minimal tiled-member
title strips that preserve pointer controls, dragging, resizing and detaching.
This is a functional visual follow-up, not a memory optimization task.

### Reusable workspace checkpoint (2026-09-07, complete)

September 7 controls follow-up: reviewed Print delegation and the real private
installed-session capture/volume proof are integrated (`a5f1fdd8`, `f8f0b504`).
The integrated controls build, shortcut gate, three runner contracts, and strict
documentation build pass. The final `0.1.0_pre20260907-r1` package is installed
from source `8486e058`. The actual installed launcher/plugin passed two-session
Save/Reopen, exact layout and manual window assignment, and identity re-save.
Native CI run `34180153305` passes both mandatory boots and the complete native
build. Release tag: `v0.1.0-pre.20260907`. Installed idle-preference
verification passes: changing 10 → 11 → default 10 through Settings1 updates
PowerDevil profiles 600 → 660 → 600 seconds through the production binding.
The real KDE privilege prompt appeared
in installed session 39; the user corroborated it, and canceling the pending
request left the agent and desktop healthy.

- Preserve named layouts and application intent across logout; provide Save
  workspace and Reopen workspace with missing-app reporting and explicit window
  assignment when several windows belong to the same application.
- Give each container a name and optional user-chosen color. Carry that identity
  through its title bar, rolled-up title strip, and single dock item.
- Add Roll up / Unroll and Iconify / Restore to container title-bar controls and
  the QindaQt context menu. Roll-up retains a compact movable title strip;
  iconifying hides the whole group behind one dock item. Restoring either must
  preserve geometry, layout, active member, and member ownership.
- Verify media keys with feedback, Print Screen, privilege prompts, and
  configurable idle display-off behavior in the installed session.
- Compile and boot the pinned plugin in CI; document KWin upgrades and publish a
  tagged, Gentoo-installable desktop checkpoint.

The stopping point is the installed behavior above, including a named workspace
surviving logout and reopening. Foundation tests and unintegrated candidates do
not establish completion. Automatic grouping rules follow this checkpoint.

Workspace foundation evidence: `f3fd6dc9` integrates reviewed candidate
`1824b3cd`; 3/3 integrated Core/workspace tests pass. This is the portable
model, storage and assignment boundary only; the checkpoint above remains active.

Capture and desktop-entry launch evidence: `2b479c6b` integrates independently
accepted candidate `df5b5a91`; 4/4 integrated Core/workspace tests pass.
Atomic adoption is integrated at `6bdde945`, and native dialogs at `343600b2`.
The reviewed KWin runtime port candidate `e64e96b6` is integrated. Session
wiring is integrated at `541cddef`; installed Save/Reopen qualification remains open.

Session crash recovery is also open: the September 7 shell render-thread crash
ended the compositor session after the old one-restart budget was exhausted.
The reviewed paced-retry replacement (`cd951c65`) is integrated and its three
process/activation/refresh gates pass. Install and verify it in the desktop;
diagnose and verify the rendering mitigation separately.

Kimi's three original CI source-size fixes are integrated and independently
reviewed. The new Terminal `main()` size error is repaired through startup
helper extraction; the integrated source-size gate now passes with zero errors.

Release infrastructure candidate `8f676db7` is integrated: exact native-stack
checks, full Gentoo desktop package definition, native build/boot CI, and
release/upgrade guides. Candidate production build and nested boots passed;
remote CI and the final combined source pin, Portage installation, installed
acceptance and tag are still required.

The combined implementation is integrated at `541cddef`, including reviewed
container identity, genuine roll-up, raise-only handling of shaded strips,
workspace UI composition, desktop controls, and session-owned PowerDevil.
The production build and 16/16 combined control tests pass. Real nested native,
portal, and legacy idle-inhibition tests pass all three modes. Workspace
persistence and UI policy tests pass; actual two-session UI reopening and
installed everyday-control acceptance remain required.

The Gentoo plan accepts the existing tuned Power Profiles provider. The final
source pin and Manifest, the apps-only package ownership transition, remote
native CI, installation, and release tag remain open. No new installed desktop
or completed checkpoint is claimed by this integration.

### Dock hover behavior (user addition, 2026-09-07)

Add a configurable dock hover mode: window/container preview, temporary raise,
or off. A container's dock item represents its whole group. Temporary raise
must not move keyboard focus, and leaving the item must restore the previous
stacking unless the user explicitly activates the item. A preview must depict
the represented window/group, not merely repeat its title. Preserve normal
click activation and fullscreen behavior.

Queue implementation after the container identity/dock candidate integrates,
so hover handling and name/color projection do not collide. This remains open;
no preview or hover-raise runtime completion is claimed.
