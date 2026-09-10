# Integration handoff

## Bundled applications on stock Qt 6 (September 10)

[ADR-0116](wiki/adr/0116-build-bundled-applications-on-stock-qt6.md) retired
the custom token UI for bundled applications and scoped QST-1/`QindaQt.Controls`
to system surfaces (shell, panels, Settings, Settings Center, Welcome). All
four bundled apps now render with stock Qt 6: the QML apps (Calendar, File
Manager) use stock QtQuick.Controls over the platform-theme palette; the
widget apps (Terminal, Text Editor) take palette/fonts from the Qt platform
theme (ADR-0115) with no token projection or application QSS. AppShell
lifecycle, action catalogs, and the fail-closed global-menu export are
unchanged seams.

Commits on `main`, each verified at its own tree:

| Lane | Commits | Content | Focused rows |
| --- | --- | --- | --- |
| Decision record | `40bd202c` | ADR-0116 + re-scoped coding-practices, module boundaries, controls gate docs | docs gates only |
| Calendar | `737ebf27`, `34013338` | untracked app stabilized and committed; stock-Controls conversion; real event editing (RFC 5545 revision, stable UID) + details pane; orphaned tests registered; wiki page; ebuild component + kcalendarcore | 12/12 |
| File Manager | `337edf3a`, `5d54c653` | stock-Controls conversion; clipboard cut/copy/paste, drag-and-drop, properties dialog, bounded recursive search through the identity-checked mutation layer | 31/31 |
| Text Editor | `504ff587`, `942b13e3` | chrome de-projection (platform theme + Fusion); printing (dialog-free PDF seam for tests); bounded crash-recovery autosave with explicit Restore/Discard consent | 27/27 |
| Terminal | `b8cfa02a`, `92cd4575` | chrome de-projection; opt-in session restore (profile id + cwd only, consume-on-launch); OSC-8 pinned unsupported with qtermwidget 2.4.0 header evidence | 24/24 |
| Gate fix-ups | `dc992d63` | font-bootstrap wiring gate re-scoped to ADR-0116 call set; Terminal `main()` decomposed under the source-shape limit | 25/25 (terminal + font wiring) |

Full Debug build passes under `-Werror`. `mkdocs build --strict`,
`tools/docs_validation.py` (221 documents), and `check-source-shape` pass with
no findings in the program's paths.

### Coverage limits at handoff

- Full-suite close-out (804 rows, `WAYLAND_DISPLAY=qindaqt-0`): 789 pass.
  The 15 failures decompose as: one stale harness expectation in
  `qindaqt.apps-native-material-matrix` caused by the editor capture rename —
  **fixed and passing in `931d2ddd`** — and 14 rows outside the program's
  ownership, all environmental:
- The five `qindaqt.settings-*-installed-route` package rows fail in this
  checkout for an environmental reason that predates the program: the
  system-wide Portage installs (September 8/9) placed `QindaQt/SettingsApp`
  modules in `/usr/lib64/qt6/qml`, so the relocation-poison stage borrows the
  host-installed module instead of failing closed on the staged prefix
  (proven by `QML_IMPORT_TRACE`: `locateLocalQmldir ... found at
  "/usr/lib64/qt6/qml/QindaQt/SettingsApp/Color/qmldir"`). The staged package
  itself is unchanged; the harness assumption "only the staged prefix can
  provide the module" no longer holds on this host. Release/settings lane
  should run these rows in a host-unset environment or teach the check to
  mask the system QML root.
- `compositor.kwin-shell-window-actions`, `compositor.kwin-plugin-dependency-contract`,
  and `session.parent-wayland.weston-headless` need a live installed
  compositor D-Bus service or a full nested stack on this host — the same
  environmental class commit `e0c7ff78` documented for the compositor lane.
- The seven `shell.notification-live.*` rows fail inside the nested session
  harness (`org.qindaqt.Display1` exits status 3, `org.freedesktop.systemd1`
  activation unavailable); the program touched no notification, shell, or
  display-service code. Routed to the shell lane with this evidence.
- Display-dependent rows (fonts coordinator, display-settings-model,
  clipboard-applet) require the session socket (`WAYLAND_DISPLAY=qindaqt-0`)
  and pass with it set.
- Editor printing claims the dialog-free PDF seam and offscreen rows only;
  physical printers are unqualified. Terminal restore persists profile id and
  working directory only — never scrollback or command content. Calendar
  reminders fire only while the app runs; no CalDAV/tasks/attendees. File
  Manager per-volume Trash, mounts, network locations, preview pane, and
  open-with remain open (S4–S5).
- The r1/r3 desktop ebuilds pin pre-program commits.
  `packaging/gentoo/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20260910.ebuild`
  pins `80760ece` (`0e44c65b` plus the installed-calendar RPATH repair) and
  is installed on the host: emerged 2026-09-10 after the operator
  archive/digest flow; the first pin (`0e44c65b`) surfaced and fixed the
  calendar's missing AppShell RUNPATH. The calendar lane already updated the
  apps ebuild component set and the ADR-0096 component list.

## Live shell refresh without logout (September 8)

After the verified r1 installation, the user requested a refresh while keeping
the current desktop, terminals and another Codex session alive. The running
supervisor was first matched byte-for-byte to the retained signed prior package;
its source includes the paced shell recovery contract. A PID-fenced SIGTERM was
sent only to the shell. The supervisor replaced shell `3481253` with `3892384`,
whose executable matches the newly installed package. Supervisor `3481239` and
compositor `3481176` retained their identities; all 52 captured terminal/Codex/
related processes survived unchanged at the immediate check. The new shell owns
the tray watcher, and Gabbee re-registered its existing item.

This refresh adopts the new shell/tray UI without logout. It does not reload the
compositor plugin, restart existing applications or replace the inherited
session environment. New File Manager processes load their installed browsing
changes. Compositor changes and session-wide Qt theme defaults still require a
later login. Evidence lives in `.cache/hot-shell-refresh/`.

The user clarified that Settings, widgets and panels should retain their
current appearance. The remaining appearance/usability criticism is specific to
File Manager; do not infer authorization for a broad Qt control reskin.

## Verified Portage delivery (September 8, r1)

**Installed:** `gui-wm/qindaqt-desktop-0.1.0_pre20260908-r1`, built from immutable
runtime `44d83ff53399d42df7ad30338515b72cf04acc67`. Independently reviewed
package candidate `f648effa` is integrated at `4d453768`. The solve updated only
QindaQt; KWin and all backend dependencies remained unchanged. Compilation
finished through Portage with 24 concurrent jobs after preserving and resuming
the objects from the initial six-job invocation.

Portage verified the new binary package's full integrity and signature before
installation. All 1,070 installed payload files match its bytes/symlink targets
and are owned by the new VDB entry. The installed Qt platform plugin passes all
five private-bus/offscreen test cases for live palette/font updates and owner
recovery. The payload includes the native KWin plugin, decoration, shell,
services and bundled apps. The prior signed package is retained for rollback.

The original session, shell, compositor and captured terminal processes retained
their process start times. No service, app or desktop restart was performed.
The user can now log out and back in to load the new code. Installed-byte and
private-fixture evidence does not establish live third-party tray actions or
resolve the unconfirmed duplicate grouped-window report below.

Evidence is preserved under `.cache/tray-package/.cache/`: the Portage build,
24-job resume and install logs, `r1-payload-verification.json` and
`r1-installed-verification.json`. The signed package is
`/var/cache/binpkgs/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20260908-r1-1.gpkg.tar`.
The combined 31 focused gates, strict MkDocs and 218-document link check pass.
The source-shape checker still reports the pre-tray `EntryGrid.qml` `qsTr`
function-span false positive; no unrelated source refactor was included.

## Focused tray and shared Qt appearance repairs (September 8)

Independently accepted tray candidate `06bc22ca108076f647cc4fc703b9dc28bdf5708f`
is integrated at `c82c60c4`. Tray context menus now consume application-exported
DBusMenu actions through the existing shared client, including checked actions,
submenus and configuration-window requests. Primary/middle clicks now reach
the item; menu-only activation, wheel input and actual global coordinates are
forwarded. Existing tray styling remains in place. All 25 tray/shared-menu tests
pass, including relocated package checks and two offscreen full-stack fixtures
with exactly one configuration action. Native QTest input lacks a compositor
popup-grab serial, so these captures are explicitly offscreen evidence. Gabbee's
menu export was inspected read-only; no live third-party action is claimed.

Independently accepted Qt candidate `bf5d9c965c79c6c261413898a3fe0a6f7a3c9e68`
is integrated through `746645f4`. The additive Qt platform theme shares the
existing palette, fonts and icon theme with standard Qt applications, preserving
explicit application and user overrides. Existing first-party skins, QSS,
layout, compositor and backend dependencies are unchanged. Five focused Qt and
session gates pass, including a relocated plugin, live Settings1 changes and
preservation of Qt-owned native service delegation. This does not qualify every
third-party custom UI or Qt 5 application.

The combined production shell and Qt platform plugin build and all 31 tray,
shared-menu, Qt platform and session-environment gates pass on `746645f4`. Strict MkDocs and
the 218-document link checker pass.

The user's current direction is to preserve the desktop they already like and
repair specific problems. Useful targeted KDE dependencies, including PowerDevil,
remain authorized. A compositor fork, dependency purge and broad application
reskin are outside this delivery. The separate unfinished QWidget styling
candidate `f6115476` is not integrated. These repairs are installed in the verified r1 Portage package above; no
session restart has been performed.

## File Manager browsing comfort (September 8)

Independently accepted candidate `3117fedd4854849b7411942bd836f32bc7638de3`
adds draggable overflow scrollbars, Ctrl+wheel and keyboard icon zoom, direct
Details/Icon view shortcuts, keyboard paging and type-to-select, mouse history
buttons, and a current-folder name filter. Toolbar and Places surfaces are flat;
compact windows retain Forward navigation. Selection identity survives filtering,
zoom and view changes. Zoom and filtering remain session-local presentation of
the existing directory listing, without introducing filesystem or persistence
contracts. Repeated view commands remain idempotent.

All 28 File Manager CTest rows pass, including the disposable installed-runtime
and private-bus menu tests. Twelve private native captures cover compact, light,
dark, high-contrast, Details and large-icon layouts at observed 100%, 125% and
150% output scales. Strict MkDocs and the 215-page documentation link check pass.
The exact candidate received independent source, regression and visual review.
These changes are installed in the verified r1 Portage package above.

The broader daily-driver request remains open: standard file clipboard
operations, drag-and-drop, properties, recursive search and the remaining
mount/network roadmap are not completed by this browsing slice. See the
[File Manager contract](wiki/apps/file-manager.md) and
[testing harness](wiki/development/testing-harness.md).

## Dock container identity repair (September 8)

Accepted candidate `a14ec749e1cfbd8ef548f49a098b530c115d22a4` gives grouped
task entries a stable symbolic stacked-window icon in the chosen container
color. Successful container rename/color edits now invalidate shell task facts,
so a chrome-only repaint cannot leave stale dock appearance. Independent review
accepted the exact candidate after both fixture findings were repaired; all
21 focused task-list CTest rows and strict MkDocs/link checks pass.

The compiled QML regression injects grouped facts and verifies four dock/taskbar
orientations, rendered color pixels, representative changes, detach projection,
and stale/suppressed member action rejection. It does not execute real tab/tile
merges. The user-reported extra live member entries remain unconfirmed: source
grouping already suppresses them, the running shell matches its installed
executable, and the production session has no development snapshot endpoint.
The repair is installed in the verified r1 package above. No restart or
native-session convergence is claimed.

## First-party material and window ownership checkpoint (September 8)

Runtime source `c1952e90a84005aa3ce3aee07cea1b6d89b6a35c` implements the
reviewed redesign. Editor owns one document per ordinary window, with independent
close consent, restore inventory and exported menus. File Manager defaults to a
visual grid with bounded asynchronous image thumbnails, concise breadcrumbs and
original artwork. Terminal retains one PTY per window and now has independently
readable ANSI, search and profile-dialog colors. All three use themed icons;
QindaQt containers retain exclusive ownership of grouping, tabs and splits.

Pearl, Smoked Plum and Velvet replace the built-in palette values while keeping
persisted theme IDs. Shared icons, opaque reading surfaces with translucent
highlights, confirmed font/accessibility inputs and high-contrast fallbacks are
covered by ADR 0109–0112. Existing accepted ADR 0098–0107 remain unchanged.

The full native Debug build passed with 24 parallel jobs. The final application
and shared-runtime selection passes 121/121. Fresh native app fixtures pass all
four observed output rows: 1080p 100%, WUXGA 100%, 1440p 125%, 1080p 150%, including
compact, light/dark/high-contrast views and actual Terminal PTY input. Independent
reviews accepted the exact app candidates and inspected the repaired captures.
These artifacts live in the ignored `.cache/qq-material/build/dev` tree.

Broader verification is **not** a clean release-suite claim. TTY and socket-path
failures were rerun with offscreen defaults and a short private TMPDIR. Five
Settings installed-route poison checks discover ambient host QML modules; six
notification-live rows fail during isolated session qualification. Fractional
full-desktop topology checks reject mismatched geometry. These gates remain
separate from the passing app matrix; do not mark the entire desktop release
qualified from this checkpoint. Source-shape checks, 310 SVG validation, strict
MkDocs and the 215-page link checker pass.

The dated `gui-wm/qindaqt-desktop-0.1.0_pre20260908` recipe pins the exact runtime
source above. Its local source archive is Manifest-verified; no public source
push is implied. Portage's plan resolves one upgrade on this systemd host.
A signed rollback gpkg of the installed September 7 revision is retained using
the existing local signing key. The new package built successfully with 24 jobs. Portage's gpkg reader verified
its required signatures and extracted its payload for inspection. SHA256:
`a8a18ee6b8a87682f9ba3daf41d5359b489f123cf3c243525b89b53d8bd923a8`.
The extracted image passes the exact KWin build/install release contract;
Editor/Terminal resolve their installed themes and File Manager's production
mutation UI probe succeeds. The actual packaged Terminal shell confirms a real
PTY on stdin/stdout on private native Wayland, and the packaged Editor keeps a
fixture document window live. Normal-scale desktop 1080p/WUXGA/dual interaction and
both panel-visibility rows pass after the reviewed Gentoo `lib64` harness repair.
**Installed and verified:** at the user's explicit direction, Portage merged
`gui-wm/qindaqt-desktop-0.1.0_pre20260908` using the verified signed binary package.
The existing desktop and worker sessions were preserved. All 1,067 installed
payload files exactly match the signed package and have the expected Portage
owner. The installed File Manager production mutation UI probe passes; the
installed Editor opens a document on private native Wayland; the installed
Terminal executes a shell with real PTY stdin/stdout. The installed exact KWin
release contract also passes. Newly launched apps use the redesigns; existing
processes retain their loaded code until restarted. Fresh-login acceptance of
the entire desktop is separate from this completed app installation.
See the [Gentoo install procedure](wiki/development/gentoo-desktop.md).


## September 7 checkpoint complete

The installed Gentoo r1 checkpoint is released as `v0.1.0-pre.20260907`.
Named workspace restoration and daily controls passed installed-session checks.
Native CI run `34180153305`, job `101917487408`, passed the complete build,
staging, release contract, three static checks, and both mandatory native boots
on `dc8a54cc`. Product source remains the installed `8486e058`; subsequent
changes are tests, packaging, CI setup, and documentation. See the
[release notes](wiki/development/release-checkpoint-2026-09-07.md).

This closes the bounded active goal. New bundled-app design work may proceed
in its separate worktree. The broader hardware/performance/migration matrices
and unrelated CI failures are not represented as complete.

## Installed idle preference verified

The private installed-session writer check passed Settings1 default 10 → user
override 11 → default 10, revisions 0 → 1 → 2. The installed desktop-controls
process changed all three PowerDevil profile timeouts 600 → 660 → 600 seconds
without a diagnostic refresh call. The earlier real inhibition checks remain
separate evidence; this writer diagnostic does not exercise inhibition.

## Installed service refresh

The real KDE privilege prompt appeared in session 39 and was corroborated by
the user. Terminating the pending request removed it without disrupting the
desktop. Settings1 was still an obsolete pre-install daemon; after restarting
it, the ten-minute idle preference matches PowerDevil AC display-off at 600
seconds. Audio, Bluetooth, Network, and Power services also had deleted old
executables and were restarted successfully through their existing user units.

## Desktop-controls integration follow-up

Reviewed screenshot candidate `3b191acd` is integrated as `a5f1fdd8` and
`f8f0b504`; its input prerequisite was already integrated. Print now delegates
to Spectacle. The private installed-session proof captured and decoded a real
region image and observed volume increase with one shell feedback popup.
The installed package is now `0.1.0_pre20260907-r1`, built from exact source
`8486e0588e8dd8c21f176be162b53d81ee73a5b6`. Portage build and merge both passed.
The two-session workspace proof passed against the actual installed launcher
and plugin, without an artifact overlay. Its reviewed regression is integrated
at `83d07911`. The native CI boot checks and release tagging are complete.

## Earlier component qualification

Main `541cddef` contains the reviewed combined workspace and desktop-control
implementation. Save/Reopen is wired into the compositor; containers carry
names and colors, roll up into movable strips, and iconify as one dock item.
The final raise-only repair keeps shaded members hidden while bringing a
partly covered strip forward; explicit activation unrolls before focusing.
Its independent review accepted `61dce8be` after the live interaction proof.

The exact combined tree builds the production desktop and session probe.
Controls pass 16/16 focused CTest rows; workspace persistence, workspace UI
policy, and chrome-manager checks pass. Real private nested PowerDevil tests
pass native Wayland, portal Idle, and legacy ScreenSaver inhibition: each
holds display-off, then permits it after release. Documentation validation
covers 209 pages; strict MkDocs and source-shape checks pass. These results
do not establish installed-session acceptance.

The remaining acceptance work is two-session Save/Reopen through the real UI,
media keys and feedback, actual Spectacle capture, a privilege prompt, and
verification after installing the complete Gentoo package. The staged Terminal
has executed a shell command with both input and output attached to a real PTY.
The staged prefix is a test artifact, not a host installation.

External Calendar changes remain outside the integrated commit. Their three
CMake edits were restored exactly after the fast-forward. The schema retains
all three Calendar preferences alongside the new idle preference; a backup
stash and the original patch are retained in the manager's ignored scratch area.

## Gentoo development checkpoint installed; release acceptance pending

The full package pinned to `d1232e75` built successfully through Portage's
compile, install-image, and package phases. Compilation resumed with 24 jobs
at the user's request, preserving completed objects. The resulting gpkg is
25,098,240 bytes. Its image contains all three bundled applications, the
production shell, session launcher, desktop controls, KWin plugin, and
KDecoration plugin. Checked application/shell dynamic dependencies resolve;
the release contract verifies exact KWin 6.6.6 and both build/image artifacts.

The local overlay and package-specific keyword are configured. Portage's
host plan resolves the desktop package and replacement of the apps-only
package with all blockers satisfied. Apps-only was deselected without
uninstalling its files; a fresh rollback gpkg was created from the installed
apps. The binary package then merged successfully and replaced the apps-only
package. Portage CONTENTS confirms ownership of the desktop and app files.
The inactive QindaQt session was ended for the merge; SDDM and TTY workers
were retained. A fresh login uses this development checkpoint.

The installed Terminal launched its shell with real PTY input/output and
executed a proof command. This offscreen check is narrower than a complete
GUI acceptance run. The pending screenshot ownership repair requires a new
final source pin, and installed-session workspace/desktop-control acceptance
and the release tag remain open.

## Shell recovery integration

Reviewed candidate `cd951c65` replaces the destructive one-restart shell limit
with paced retries capped at 30 seconds. The notification host and applications
remain resident across repeated shell failures; initial startup failure,
explicit stop, host exit, and compositor death retain their terminal behavior.
The integrated process/activation/refresh gates pass 3/3, including the full
70-second supervisor test. The basic render-loop default and renderer diagnostics are also integrated;
a private OpenGL/llvmpipe run recorded the basic loop and all three shell roles.
Installation and physical-GPU qualification remain pending; the mitigation is
not a claim that the render-memory fault is repaired.

## Native release and Gentoo package definition integrated

Reviewed release candidate `8f676db7` adds the full desktop ebuild, exact KWin
source/ABI checks, native plugin build and boot CI, and release/upgrade guides.
The candidate's fresh full production build and two nested plugin boots passed;
the manager verified its build/install contract and remote KWin source objects.
The integrated documentation passes validation for 202 pages and strict MkDocs.

The reviewed ebuild and Manifest now pin integrated source `d1232e75`
(package integration `80d88779`). Archive size, both digests, and selected
production source bytes were independently verified. Any further accepted
production repair requires a new source pin and Manifest before installation.
Remote CI, the full Portage merge, installed-session acceptance and the release
tag remain pending. The definition is integrated; no new desktop installation
or qualified release is claimed.

## CI source-size blockers repaired

Kimi's product candidate `d4f65d84` and handoff `61c39519` passed independent
Luna review. The Global Menu action delegate is a registered and installed QML
component; Gabbee CMake rows and process-start helpers are split without
changing test names or startup order. The limits were not relaxed.
Root reran the integrated Gabbee units: 26 passed; the PTY suite ran 21 with
two documented real-sink skips. Candidate offscreen QML coverage passed 8/8.
The integrated checker now passes with zero errors after the Terminal startup
helper repair (`abe3413a`). That repair preserves profile/argument handling and
reduces `main()` to 179 lines. Remote native CI remains unpassed; no release
qualification is claimed.

## Terminal now leaves tabs and splits to containers

Reviewed and repaired candidate `e92243f0` removes the internal tab strip and
tab actions. New Terminal launches a separate process with the selected saved
profile and observed working directory. Root reran six focused window,
container, action-catalog and private-bus menu tests: 6/6 passed. The integrated
Terminal source/tests match that exact candidate; its dependent app-shell,
theme and settings sources have not changed since the candidate base.

The first review caught an obsolete menu action in the test fixture; the
repair uses `file.new-terminal` and the real private-bus activation test passes.
Combined release build and installation remain pending. The installed Terminal
still has the previous interface until the full Gentoo package is updated.

## Interactive Terminal fixed; Gentoo package installed

This supersedes the earlier Terminal installation claim below. The earlier
checks proved process startup and plain output, but missed the normal Gentoo
interactive prompt. Silent bell handling removed BEL, which also terminates OSC
window-title sequences; the renderer swallowed the prompt and later output.
Fix `3ec80588`, integrated at `89ad375d`, preserves PTY bytes and handles sound
only through parsed bell notifications.

The complete TerminalWindow regression now starts an isolated interactive Bash
with a BEL-terminated prompt, enters a command through widget keyboard events,
and verifies searchable output and painted glyphs. It passes on Wayland; the
real-widget suite passes 8/8 cases and six focused Terminal CTest rows pass.

`gui-apps/qindaqt-apps-0.1.0_pre20260907::qindaqt` is installed through Portage
from the immutable fixed commit. It owns File Manager, Editor, Terminal and
shared QML runtime files, declares slot-bound dependencies and is in world.
All 40 installed object checksums match Portage's CONTENTS. Executable RUNPATHs
resolve under `/usr`; no build/source paths or missing libraries remain.
The original `--buildpkg` attempt hit unconfigured local GPG verification; a
source-only resume merged successfully without changing binary trust policy.
Portage adopted the previously unmanaged files and reported no owning-package
collision. [Gentoo installation](wiki/development/gentoo-apps.md) documents updates.

A fresh installed Terminal launched with no options and published the normal
`cabewse@qinda container-wm` shell title through the compositor inventory. It was
left open for the user at verification time; the later shell crash ended that session. Installed File Manager UI actions pass. Installed Editor
reports a 46 ms first frame and 17,160 KiB median PSS. Evidence and the fixed
window capture are under ignored `.cache/terminal-visible-install/`.

The main checkout contains unfinished Calendar edits owned by the separate
worker, so its full reconfigure currently fails on missing Calendar CMake files.
Those edits are preserved. Verification and the Portage build use the exact
committed snapshot, excluding that unrelated working-tree state. Documentation
validation and strict MkDocs pass for 193 pages; the previous three unrelated
source-shape violations remain outside this fix.

## Bundled applications installed — September 7

The primary assistant personally implemented and checked candidate `4dfbe5a2`,
integrated at `44fff730`. The user explicitly requested no delegated coding;
no independent worker review is claimed.

- Text Editor: line numbers, syntax highlighting, current-line and cursor
  position, Go to Line, undoable indentation, auto-indent, per-tab wrapping and
  zoom. KF6 SyntaxHighlighting is installed; ADR-0095 confines it to presentation.
- Terminal: preserve monospace fonts on theme changes, per-tab zoom, and new
  tabs in the active shell's current directory. Live PTY input/output and owned
  process-directory observation are tested.
- File Manager: Kimi's S2 plus the selection and keyboard repairs described
  below are rebuilt with the same integrated dependencies and installed.

Main passes **66/66** app CTest rows, including global-menu composition and
staged packages. Installed `/usr/bin` binaries match the main build. Native
Wayland launch checks pass for Editor and Terminal; Terminal's child confirms
TTY input/output and its working directory. Editor's installed offscreen first
frame is 51 ms and median PSS is 21,954 KiB, within the existing 400 ms/65,536 KiB
limits. The actual installed File Manager UI-action probe passes.

Evidence is in ignored `.cache/apps-integrated-*.log` and
`.cache/apps-installed-proof/` (hash manifest, launch logs, runtime measurements
and previous executable backups). Documentation validation passes for 191 pages,
strict MkDocs and whitespace checks pass. The source-shape checker still reports
three violations already present at base `67b61c61`: GlobalMenuApplet.qml length,
tests/session/CMakeLists.txt length, and `_run_inner_phases` length in
`tests/session/gabbee/gabbee_terminal_boot.py`. No new violation is introduced.

The existing desktop session was preserved. Launching an app again uses the new
binary. Kimi separately owns Calendar. File Manager's later icon/preview,
mounting and SMB slices remain on its documented roadmap.

## File Manager S2 integrated and installed

Kimi’s `0ab8cd75` plus primary-assistant repair `01e41ede` are integrated
at `0f280019`. The user requested the primary assistant do this personally;
no independent review of the repair is claimed. The original independent
findings are addressed by shared identity-based selection, navigation clearing,
list/grid keyboard parity, stable focus, and meaningful batch confirmations.
All 22 File Manager rows pass on the integrated tree, including real QML
actions, global-menu routing and staged installation. The installed executable
matches the main build and its actual `--check-ui-actions` probe passes.
Documentation, strict MkDocs and whitespace gates pass; the current source-shape
qualification is recorded above.

The Terminal and Text Editor follow-up is now installed, as recorded above.

## Bounded usability checkpoint complete

The September 6 reported-task checkpoint is complete and deployed. The final
requirement table and qualification limits are in [Task list](TASK_LIST.md).
The user confirmed setting the Samsung 4K monitor to 125% through Settings;
independent readback shows HDMI-A-1 scale 1.25, DP-1 scale 1.0 at x=3072,
and no pending transaction. The manager did not change that preference.

Both packages match their manifests, the shell and six services were refreshed,
and Settings plus its service and appearance portal run the new binaries. The
compositor plugin is byte-identical to the one already deployed for this session.
Closing tests pass 661/661; native interaction and hardware evidence are scoped
precisely in the task table. No additional feature work or logout is needed for
this checkpoint. File Manager S2 has since been repaired and integrated, as recorded above;
mounts and SMB remain later file-manager slices.

## Final dual-output diagnosis

Run `085a865f3f804842b342c06f088e0213` verifies actual member-title
hide/restore and group minimize/restore using the measured context-menu row
relative to its current anchor. The remaining scale failure is explained by
the exact KWin 6.6.6 source: its nested Wayland backend ignores requested
scale. Settings sends 1.25 correctly, while the nested output remains 1.0.
The source downloaded from upstream matches the local release archive byte
for byte. Physical preview/Keep/Cancel scaling evidence remains valid and
separate; no physical Settings-pointer cycle is claimed. See the
[testing harness](wiki/development/testing-harness.md) for the precise boundary.
The private run restored its frozen harness files and left zero survivors.

## Gabbee command shortcut activation verified

Private run `29894db03d7d49d2ad9766a346b3f181` passes all four phases
against the real KDE GlobalShortcuts backend. The actual Gabbee binding receives
exactly one command pressed and one command released callback from injected
F11 input, with the duplicate-observation window retained. This complements
the earlier push-to-talk F1 activation proof. User-specific F23/F24 bindings
and physical microphone transcription are outside these synthetic-input checks.

The manager also compared the newly installed compositor plugin with the
pre-install backup: they are byte-identical. The existing compositor session
already loaded the accepted changes; this refresh does not require a logout.
A fresh host screenshot was unavailable through both Spectacle and the
standard screenshot portal (response 2); no successful host capture is claimed.

## Installed checkpoint and component refresh

The user entered sudo credentials in a native terminal. Both installers
completed; independent post-install hashing matched all 1,077 QindaQt and
3,181 Sloom entries. The manager removed the temporary Display override and
restarted the six resident services. Every service returned a snapshot; no
recent user-service errors were logged. The shell restarted at 08:33 MDT on
September 7 as PID 2205657, with the original compositor and session preserved.
Settings, its D-Bus service, and the appearance portal were refreshed afterward.

Final running-desktop interaction checks and any required compositor refresh
remain outstanding. Installation is no longer blocked on credentials.

## Host shortcut configured

The manager configured this user’s previously unassigned note shortcut through
KGlobalAccel as Meta+Shift+F1. A preflight query showed no owner for that key;
readback returns `318767152`, and `kglobalshortcutsrc` persisted the sequence.
The previous configuration is preserved in `.cache/shortcut-note-host-binding`.
The running old shell still advertises the old default; the staged build has
the corrected card and default. This host check proves binding and persistence,
not physical key activation.

## Combined deployment prepared

The existing `.cache/session-checkpoint-current/install.py` now resolves to
a combined installer. It verifies both payloads before installing either:
1,077 QindaQt entries and 3,181 Sloom files/symlinks, including file modes.
The Sloom installer copies the complete package into a staging directory,
verifies it, then replaces `/opt/sloom-studio` while retaining the previous
directory as `/opt/sloom-studio.before-qindaqt-f459b0fd`. Both installers
write installation receipts and leave process restarts to the manager.
Verification-only execution passes; no system installation has occurred.

## Final native checks and Sloom package inspection

The compact Settings check is complete: run
`d7a0ca34b3234d3ea1de20b892756ef8` reaches Customize by keyboard and
reveals its tab after shrinking the window. A notification popup overlaps part
of the capture; the independent five-case Qt geometry test supplies the exact
viewport-fit evidence.

Sloom candidate `f459b0fd3b22e33047590ae5d307a6c410d3fc03` passed
independent source review and 20/20 focused tests. Native run
`a20d3aed75694d879a0a1cfab499c033` opens Help → About through real pointer
input with no duplicate window menu. The manager inspected the About capture,
verified successful cleanup, and compared all three changed production files
inside the packaged ASAR byte-for-byte against the accepted commit. Its SHA-256
is `1cc9658d2b57edf2d2826e6dab0017a21bbccc0075ea87f2fe0a8c91ce53e7ad`.
Live host withdrawal was not exercised; focused lifecycle tests cover fallback.
The additional package-review worker stopped at a provider usage limit; no
independent package-audit result is claimed.

QindaQt and Sloom installation, the legacy note binding adjustment, and checks
in the refreshed host session remain outstanding. The staged QindaQt payload
still passes 661/661 closing tests; sudo currently requires a password.

## Updated checkpoint and shortcut proof

The current integrated tree `29be28fd` passes the closing selection again:
**661/661**, exit 0, 129.73 seconds (`.cache/finish-closing-29be28fd.log`).
The new 1,077-entry payload `.cache/session-checkpoint-29be28fd` is staged and
manifest-verified; the previously supplied current installer now resolves to
it. No system installation has occurred.

The note default is now Meta+Shift+F1 because KWin already assigns Meta+F1 to
Desktop 1. Independent and integrated focused tests pass 2/2. Private run
`b752e061dc2a402f9729ab08b9606fca` proves the exact active/default binding,
then real keyboard input changes the dismissal preference false → true → false.
The manager inspected the visible card's full corrected shortcut label.
Existing remaps and disabled bindings remain preserved by the product. This
user's unassigned legacy note binding still needs deliberate configuration
with deployment. The run's overall failure was a separate compact-hook route
expectation; the shortcut evidence itself is complete.

Compact tab visibility is integrated at `1d6d00f1`, independently tested 5/5
and integrated-tested in all three navigation rows. Its final native capture
remains separate. The Sloom staged package now proves global Help → About via
real pointer input (run `b1bc50b9400d45168d7819393e06b70b`); its duplicate
local menu exposed an observer using a well-known rather than the required
unique exporter name. The isolated Sloom repair is being requalified.

## Menu labels and Settings native evidence

Integrated `575e30d2` keeps admitted top-level menu names complete while the
existing whole-entry `+N` policy handles limited space. The manager reviewed
exact candidate `8182b752`, independently passed 14 applet, 12 overflow and
3 vertical checks, and inspected the full `Tabs` label in private run
`5b76fbf122f043bbae405a34fd055ba1`. The integrated module build and all 10
menu QML/boundary CTest rows pass. This production change is not yet installed.

That run also provides inspected Appearance destination, Color capability,
Clipboard popup theme, compact route activation and Power controls evidence.
Power changed the timeout to 15 minutes and restored the original disabled/5
minute preferences. The overall run remains failed: Got it dismisses the note,
but Meta+F1 does not reopen it. The compact screenshot also exposes a distinct
remaining defect: keyboard selection reaches Audio without scrolling its tab
into view. Both are assigned focused follow-up work. Private files and both
temporary menu-library overlays were restored, with no surviving test processes.

Sloom's real exporter and native Help popup are verified in run
`fd96e399e6524357ac956c5e9ed886ca`; About action acceptance remains open.
The capture shows a duplicate local menu. A separate Sloom worktree is adding
consumption of the existing ADR-0077 hosting acknowledgment; no new QindaQt
protocol is needed. Failed hook calls and startup-only runs are not product
failure evidence.

## Updated closing suite passed

The integrated popup controls, Gabbee test registration and private font
configuration pass the updated closing selection: **661/661**, exit0,
129.78 seconds (`.cache/finish-closing-661.log`). The versioned deployment
payload is `.cache/session-checkpoint-375dc660`, with all 1,077 entries verified.
The previously supplied `.cache/session-checkpoint-current/install.py` command
resolves to that installer. System installation remains outstanding; the
Settings/Sloom native acceptance runs are the remaining active runtime work.

## Native Gabbee shortcut routing verified

Private run `bd01eade1a1442d7ac9bc4147ac293d2` passes 4/4 phases against the
real `/usr/libexec/xdg-desktop-portal-kde` backend. Both `push_to_talk` and
`command` register using private test bindings F1/F11. An F1 press/release
through the compositor keyboard path produces exactly one Gabbee push-to-talk
pressed callback and one released callback, with a duplicate-observation
window. Manager inspected the real backend executable evidence, registrations,
input route and callback events. This does not assert that the command binding
was activated or that physical microphone capture was tested.

Private font configuration `6b628be9` passes independent and integrated focused
checks (10/10 each), including real fc-match parsing. Its minimal environment
patch and fixture are copied to the frozen visual runtime with the original
file preserved under `.cache/fontconfig-frozen-backup`. The private shell stage
is refreshed to popup-controls `375dc660`; Settings visual acceptance is next.

## Popup controls follow the shared theme

Integrated `375dc660` completes the themed-button repair for Clipboard Close,
Bluetooth pairing Confirm/Cancel and Power profile/session actions. Root reviewed
the exact candidate and independently passed all 13 affected checks; the
integrated shell build and the same 13 checks pass. The existing availability,
accessibility and action guards are preserved. Strict MkDocs and documentation
links pass; final native visual capture and installation remain outstanding.

## Gabbee sink live acceptance passed

The real `gabbee.agent_input.AgentInputTextSink` passes both standalone and
group-member Terminal delivery in private runs `bd2ad5a9fa424203ab552f7075a90834`
and `6fbbe1a2f3f0485880eb4925cf057cf8`. Each run passes 11/11 phases, with
`method=agent-input`, successful non-uncertain delivery, separate unique Unicode
sentinels and actual shell-written files. Manager inspection independently
compared all four files with the expected bytes. Each sink session used the real
portal approval; direct helper sessions carried only framing keys and pointer
focus. Both runs clean up. This closes the terminal adapter delivery gate for
integrated `f23f6710`; it does not claim a physical microphone transcription test.

Native panel fast-click run `b9959bfb91df43e49b5a5d4568aafcb4` uses a measured
93 ms click at the lower File-label edge, retains the popup after release and
one second, then adds exactly one tab through New. Separate native captures
verify lower-edge Clipboard and Bluetooth popup opening. The top menu Tabs
label remains visibly elided and has a bounded repair owner.

## Gabbee sink candidate accepted for integration

Independent GLM review accepts exact `fc8723a1` after rechecking all three
blocking findings: the real external sink now sends the sentinel, the wiki
corrects its earlier host-uinput claim, and the unit/syntax rows are registered
with CTest. Source decomposition keeps the runner and collaborators cohesive.
Integrated verification passes all three CTest rows and 47 discovered
unit tests against the actual Gabbee checkout; strict MkDocs and link validation
pass. The live sink run remains required; the earlier direct-helper readbacks are
not substituted for it. The actual external Gabbee checkout now passes its
complete 187-test suite (one GI deprecation warning), beyond the 44 focused
checks previously recorded.

## Adapter and portal verification update

Gabbee focus restoration now precedes its terminal helper route; the applied
external source passes 44/44 focused checks, including failed focus restoration
and uncertain-delivery no-replay cases. Candidate `cd0d4465` proves byte-exact
Unicode PTY readback through the real portal helper in standalone and grouped
Terminal windows, twice. Inspection confirms this fixture calls the helper
directly, so it does not yet qualify the Gabbee text sink itself. That adapter
path is the next required proof, and the candidate remains unintegrated.

The live portal pair was refreshed without logout. Both connect to `qindaqt-0`;
the frontend identifies QindaQt and exports RemoteDesktop, GlobalShortcuts and
ScreenCast. The KDE backend intentionally retains its KDE-specific desktop
identity. Startup repair is integrated through `e360059e`, after independent review
and correction of backend facts and asynchronous restart wording. The integrated
session build and three supervisor/environment/refresh tests pass; strict MkDocs
and documentation links pass. The updated session binary still needs deployment.

## Closing suite and prepared system installation

The updated closing selection passes **660/660** on integrated `8179a0f9`
plus documentation-only follow-ups. The complete staged payload contains
1,077 files/symlinks, verified against its manifest. The prepared installer is
`.cache/session-checkpoint-current/install.py`; `--verify-only` validates the
payload without privilege, and running it with sudo installs the staged files
with backups and post-copy checks. It does not restart the graphical session.
The expired passwordless sudo grant prevents the manager executing this system
installation. The user-level Display service remains deployed and verified.

The Gabbee worker's claimed absence of a Weston backend and earlier portal
proof was disproved: `usr/lib/libweston-15/headless-backend.so` exists, and run
`2a2db7b6777e487b8694c5275d51443e` has an exact Unicode expected/readback match
with helper READY and exit0. Its remaining terminal proof has been redirected
to that known working private Weston setup; no environment blocker is accepted
on the contradicted report. Candidate `f1187bad` is preserved but unintegrated.

## Panel hit-target repair integrated

`8179a0f9` removes invisible interactive scrollbars from the panel's click
areas and corrects cross-axis allocation. The exact candidate passed 20
independent checks; the integrated shell build and the same 20 checks pass,
including lower-edge/corner menu clicks, status controls, vertical layouts,
overflow and dock geometry. Strict MkDocs and link validation pass. The
running shell has not yet been replaced, so the user still sees its old panel.

Gabbee's applied helper integration still has 41 passing focused tests.
The private terminal proof has not reached input delivery: its compositor
setup lacks the portal's screencast interface. The implementer is repairing
only that private launch configuration, using the earlier successful portal
fixture as a reference. No terminal insertion completion claim is warranted.

## Physical scaling verified and user-level deployment complete

Integrated `347eb03f` retains valid inventory events that arrive before the
independent apply acknowledgement. The repaired candidate passed independent
and integrated transaction gates (five groups each), strict MkDocs, and the
link checker. The physical HDMI-A-1 preview reached 125% and AwaitingConfirmation;
Cancel restored both displays and cleared the transaction. A separate Keep
cycle confirmed 125%, then a second confirmed change restored the original 100%
configuration. Evidence is in the manager's live-scale-proof result and
confirm-result artifacts. This is direct Display1/compositor evidence; it is
not a screenshot-based Settings interaction claim.

The service is now deployed to `$HOME/.local/libexec/qindaqt-checkpoint/` with
a persistent user systemd ExecStart override. Its bytes match the verified
build (SHA256 `3b58f66f14704fba247b2541df6e2e63322844e258dc314dd6141abaec841317`).
The temporary build-directory override and Wayland tracing were removed.
The system-wide `/usr/bin` copy has not been replaced: passwordless sudo has
expired. Session-startup refresh `7d2c8f8a` still awaits deployment.
No full graphical-session restart occurred during these checks.

## Live Display connection repaired — 2026-09-06 21:04 MDT

The old Display service PID 1780106 retained `WAYLAND_DISPLAY=wayland-0`
from KDE, despite the current QindaQt shell and Settings using `qindaqt-0`.
The systemd activation environment was already correct; it had not updated the
existing service. Restarting only Display at 21:04:35 produced PID 2012983
with `WAYLAND_DISPLAY=qindaqt-0` and a fresh public Display1 snapshot.
There was no logout. The session-lifecycle repair is integrated through `7d2c8f8a`, with independent
and integrated focused tests passing; its new session binary is not yet installed.
It refreshes Display and Clipboard after publishing activation environment to
prevent this mismatch. A subsequent physical preview exposed a second defect: Wayland
reports HDMI scale 1.25, but the QindaQt inventory remains at 1.0 and triggers
rollback. A further direct Compositor Outputs read proved its scale and
generation are correct. The transaction machine rejects observations during
Applying, so an inventory event preceding the independent apply acknowledgement
can be lost. The compositor-notification candidate was stopped; the transaction
ordering repair is assigned in `codex/display-observation-order`. Preserve this
distinction: do not integrate the earlier speculative notification or UI patch.

The verified `3591a081` Display runtime is temporarily running from the canonical
build through a user runtime systemd override. It clears the formerly stuck
no-op rollback correctly; the latest probe restored both original output
positions/scales and left no transaction. The override also enables temporary
Wayland tracing; remove tracing when diagnosis is complete. This is not a
system-wide installation of that newer binary.

The reviewed Gabbee terminal helper integration is now applied to the external
checkout after baseline hash checks and backups. All 41 focused tests pass on
the applied copy. Actual standalone/grouped terminal PTY readback remains open;
the terminal-proof worker owns the private runtime lane. The manager must not
launch another private compositor concurrently.

## Graphical refresh confirmed — 2026-09-06T20:21:09-06:00

The user is back in QindaQt. KWin PID 1981017 started at 20:17:41 MDT;
qindaqt-session PID 1981273 and qindaqt-shell PID 1981276 started at 20:17:42.
The shell maps /usr/lib64/libqindaqt_global_menu_qml.so. Installed shell, menu
library, and compositor hashes match the independently verified 1,077-file
payload. The earlier SDDM restart did not complete automatic QindaQt login;
its temporary autologin configuration is gone. Do not interrupt the current
session again during verification. Display and Network services report active.

The closing suite now passes **658/658** on integrated `48a5b58d` after the
AUTOMOC and navigation-fixture repairs. The strict documentation build also
passes. The no-op rollback repair is integrated through `3591a081`; it does
not establish successful scale application and is newer than the installed
Display service.

Two fresh physical-session reports remain open: selecting display scaling
returns to 100%, and the lower portion of global-menu labels/status controls
is difficult to click. Separate Claude CLI workers own those repairs in
isolated worktrees. Gabbee terminal delivery is undergoing candidate review;
its new helper sink has not yet been applied to the external checkout.
The external Kimi file-manager worker was interrupted by the earlier logout;
the user restarted it and the manager posted a reconnect message in its
existing shared thread. Its paths remain reserved. No new session restart
is planned while these workers run.

## Host refresh — 2026-09-06 18:20 MDT

Installed the complete a3be5ee0 production runtime: 1,077 files independently hash/link verified. Six QindaQt services restarted at 18:17:35 and report active. Agent-input acknowledgement repair 9b60cf7c was separately installed after independent 25/25 tests. The manager is transitioning KDE to QindaQt through SDDM. Read actual session/process state before claiming graphical restart completed.

Closing suite: 654/658, four test-fixture/scanner failures repaired through a822d5b9, but test build still needs StubScreenLockSettings AUTOMOC header registration (Terra owns repair). Production runtime unchanged by those test fixes. Sol owns actual scale Apply/no-op rollback bug in isolated display_transaction paths. Luna and Terra coordinate Gabbee helper ACK sink; external sink snapshot is not yet applied. Grouped editor insertion b3ac957acaf64dfdbf8b429919c59513 passed direct AT-SPI with no clipboard fallback.

Fresh native run a1bdb644aa3f4cdebd6a174f5ad7d7d9 shows complete menu labels, correct dark/amber context palette, arrangement Apply/Revert and group minimize/restore. Scale transaction stalls and rollback falsely becomes Stuck; not accepted. Goal remains active.

## Current integration and runtime ownership — 2026-09-06 17:56 MDT

The original goal task is the sole integration, shared-stage, runtime, and
host-deployment manager. The parallel completion task has stopped its workers;
its preserved candidates remain available. The independent file-manager worker
retains its paths.

Screen-lock candidates `5653b576` and `795198e3` are integrated through
`94b302ea`. The manager independently reran all seven focused tests successfully.
Power settings now exposes automatic idle locking and a duration selector that
retains custom values. The host Autolock preference is already false; the new
Settings UI still awaits the desktop installation.

Native-menu run `de9f130640494649993e7001d75810a7` uses the rebuilt library in
both staged library locations. Manual screenshot review shows File anchored
under its label and remaining open after release and a one-second wait. Pointer
New adds exactly one tab (one to two); keyboard New adds exactly one more (two
to three), without modifying its content. The automated semantic check cannot
see accessibility nodes and remains false; this is scoped manual evidence,
not an automated pass. The Tabs label still clips and is being repaired.
Earlier run `3177910e6a73461c8c58af2cd5ccd030` remains rejected.

Display writer `976ae25d` is integrated. Run
`518c44670d5f4ab3b3613924c978fcea` uses the rebuilt Display service and proves
actual arrangement Apply/Revert plus whole-container minimize/restore. Its scale
interaction did not complete, so overall Display qualification remains open.

Approved Unicode insertion was read back exactly in run
`2a2db7b6777e487b8694c5275d51443e`; the outer run failed on a process-sampling
race. Gabbee app/terminal/group direct insertion remains under qualification.
A minimal GI Text call correction was applied to the existing dirty external
Gabbee checkout with a complete pre-edit backup; unrelated changes were retained.

There has not yet been a complete host desktop install or session restart.
A closing build is running; accepted palette/menu candidates and remaining
runtime checks must be recorded accurately at deployment.

## Completion restart after connectivity report — 2026-09-06

The user returned to KDE and requested completion of already-started work,
without added scope. Current integrated source is `963b2893`; the deployed
QindaQt session still needs the final accepted refresh. The remaining gates are
native menu switching/keyboard behavior, approved input and Gabbee insertion,
the preserved screen-lock Settings candidate, actual Display/Color/chrome
acceptance, the closing combined suite, and installation verification. The
independent Kimi worker retains all file-manager ownership.

Native Alt-Tab run `5f700906669c498ab4889da621667fc7` passes forward/reverse
selection and grouped representative activation, with its production package
present, screenshots inspected, restored harness files, and no survivors.
The two-monitor Settings discovery run `812814054ac9409a9133b9f9f5047323`
fails before acceptance: the private Display state root is invalid and the
unarranged group-drag fixture does not converge. Neither run proves physical
monitor or ICC acceptance.

The native-menu repair is independently reviewed and integrated through
`a30fe46b`. The reviewer found inherited KDE input behavior, stale dynamically
created menu colors, and initial selection of disabled/separator rows; repaired
candidate `1a74ff5e` passes the manager's seven focused QML rows and its module
build. The combined integrated selection passes **658/658** in 114 seconds,
including current Display arrangement, grouped geometry, Gabbee unit, and
switcher/package checks. Installed/private-desktop rows remain separate gates.
The Qt/KWin accessibility package transaction has completed, and private
AT-SPI now enumerates the editor and actual portal approval dialog. Final
approved Unicode delivery and Gabbee insertion remain unproved.

Read-only NetworkManager inspection reports current connected/full state.
The recorded 16:53:40 disconnect came from live KDE `plasmashell` PID1600891,
followed by successful reconnection at 16:53:55. The user places the outage shortly before switching sessions, with their phone
unaffected. Resolved logs show the upstream DNS feature downgrade at 16:40:37
and repeated DNSSEC validation failures from 16:40:39, while NetworkManager and
kernel logs show no preceding Wi-Fi disconnect. DNS lookups succeed after the
KDE reconnect; the cause of the resolver failure and any QindaQt involvement
remain under investigation. Host networking has not been changed for this audit.


## Installed-session completion in progress — 2026-09-06

Runtime update: both normal Meta+Shift grouping and late Shift now pass in
private runs `7bf2271d708648ee99e1afd80493ef40` and
`d90a2e7ddd8747a38dea9e06dc106bb9`. Editor and Settings form a real hybrid
container, their actual frames converge, Meta+Arrow stays container-local, and
standalone Welcome still quick-tiles independently. Both runs clean up fully.

Native pointer lock, confinement, and relative motion pass in private run
`d8f380054b0c44fe8b176692063e6e11` after integrated relative input support
`aadd1497`. The native client received relative motion while locked and
completed its lock/confinement lifecycle; no private processes survived.
This qualifies the representative protocol client, not every game.

Native menu placement and File → New pass in actual run
`e338dd4684cd4a2cb8dbe492d8868686` after `e5955ea8`. The later switching repair
through `de74a9d1` passes 39/39 integrated menu checks, but actual run
`7e660a7d33c14f94a683629975678cb6` still closes Edit without opening File.
Sol is replacing the custom menu-bar event handling with Qt's native MenuBar/
Menu behavior while preserving the public facade and action-generation checks.

The installed input helper and backend-only KDE compatibility are deployed;
the live RemoteDesktop frontend reports device mask 7. Private input remains
unqualified. Exact-owner startup avoids an auto-activation race. Private run
`0dcf0eef1d564c8cbb5a200951f2dea8` proves a healthy isolated PipeWire core and
an actual Remote Control approval window. The fixture also needed an XDG
applications menu: without it KApplicationTrader returned zero portal entries
despite the installed desktop file; with it, the exact executable and declared
Wayland interfaces resolve. No permission checks were bypassed. Approval-control
accessibility and actual helper text delivery remain open; the run cleaned up
all private processes. Run `d7900d12f34b48a0aa167e76d9609573` still finds no Qt
AT-SPI applications. Host Qt headers confirm `QT_FEATURE_accessibility_atspi_bridge`
is disabled; the installed Gentoo qtbase USE flags agree. A coherent exact-version
accessibility rebuild is being planned with its Qt/KWin consumers.
Gabbee probe repairs are integrated at `088a4a4b`, with 24/24 unit tests and
strict post-delivery AT-SPI proof. Actual app/terminal insertion remains open.

The physical DP-1 projector is enabled beside HDMI-A-1. Disabled-display
inventory and output ordering are integrated through `358eff0e`, with fixture
correction `0cbde61f`; 51/51 Display checks pass. Fable's visual arrangement
canvas, completed by Terra and independently reviewed by Sol, is integrated
through `86342441`. It provides logical-size display tiles, drag/snap, keyboard
placement, and per-display scaling through existing reversible transactions.
The combined Display-page/arrangement and switcher/defaults gates pass 5/5.
Physical multi-monitor Settings qualification follows the session refresh.

Dock maximize and restore now pass the original native reproduction after
`04954961`: run `0c4d7868b85b44469417c4d28da7398a` verifies expansion when the
72-pixel dock reservation disappears, contraction when it returns, a second
expansion, and exact original actual/target frames for both restored members.
No private processes survived. Focused integrated placement/reconciliation
checks pass 3/3.

The QindaQt TabBox presentation and absent-key edge/corner defaults are
integrated through `daf86268`; explicit user choices remain intact. Native
Alt-Tab visual and activation qualification is in progress. Live grouped
windows already expose one task/switcher representative per container.

Native grouped fullscreen passes in run `0aca8a74926c44f98def9b87b2a83328`
after independently reviewed repair `511ac79b`. The owner fills 1920 × 1080;
a competing peer request restores peer geometry and preserves owner activation.
Alt-Tab activates the standalone Settings window; native fullscreen exit restores
both actual member frames to their committed targets and preserves that exact
outside window's focus. All private processes clean up. The integrated production
compositor builds and policy/package gates pass 2/2. This is a controlled native
client scenario, not a claim that every video player or game is qualified.
The earlier Alt+F11 probe was invalid because private KWin leaves it unbound.

The user's separate Kimi K3 Max worker owns file-manager work, including SMB
browsing and filesystem mounting. Shared coordination is in
`ops/team/messages/file-manager-kimi-k3-max/`; this team does not infer that
worker's liveness or completion from the user's ownership notice.

The broad integrated selection at `5400320e` passes **640/640** in 97 seconds.
It excludes installed/private-desktop rows and the already identified Gabbee
fixture repair; those gates are handled separately. Source-shape validation
passes across 2,803 files. Later menu/display/assets candidates require their
focused integrated checks before deployment.

## Earlier source checkpoints (historical)

Reviewed layer-shell exclusion and development keys are integrated through
`57b01211`; focused admission/parser/note checks pass 3/3, and the input-device
teardown gate passes separately 1/1. Navigation/transport test decomposition is
integrated through `04feefda`, with four focused tests passing. Source-shape
validation now reports no errors across 2,785 files. Native pointer test support
is integrated at `e73331b3`; it does not claim completed runtime qualification.

Current source checkpoint `d2be019f` builds successfully after independently
reviewed aggregate-fixture repair `a4d552b2`. Integrated Settings/Controls gates
pass 33/33; menu/chrome selection passes 31/31; portal/fixture selection passes
5/5; Sloom/editor selection passes 2/2. Strict MkDocs and 181-document validation
pass. Private Settings run `11cadca49f1f4f5f81f78b065db19ad6` captures ten routes;
Network guidance is compact and top-aligned, with the accepted quieter status
surface and distinct headings. Gesture runs stopped during probe preparation,
so grouped interaction runtime acceptance remains open.

Sloom selector `d2be019f` preserves native KDE address priority and uses the
existing standard exporter only for exact Sloom identities. A fresh live launch
on September 6 produced a KWin `signal-loom` window with PID `1371076` and
`org.signalloom.PanelMenu` owner PID `1371076`, an exact match obtained through
Gabbee's real KWin script bridge and the public D-Bus PID query. The bridge also
successfully discovered this live QindaQt window. Panel action qualification
still follows the compositor refresh. Gabbee GlobalShortcuts routing is
integrated at `3ad28017`; synthetic insertion qualification remains open.

ChatGPT's reported crash reproduced on its X11 backend with an isolated profile
(status139/new core). Native Wayland survived the bounded 12-second comparison.
The reversible user launcher now selects Wayland when both the Wayland session
and display are present; normal repaired launch survived with no new core.
This is an app-backend workaround, not evidence of a compositor-wide defect.
The original launcher is preserved in ignored crash-diagnostic artifacts.

Latest integration update: GitHub `Es00bac/QindaQt` now contains the tested
`ce63d1bb` checkpoint; `git ls-remote` independently confirmed that exact remote
SHA after a non-forced push. The menu repair passed 35/35 focused checks. Private
run `89259c6045c7468db7a3e3b36a2f142f` verifies a File → New click creates a second
editor tab, with no private survivors. The physical shell still runs the earlier
refresh; the new menu implementation is not yet deployed.

Reviewed Settings hierarchy/recovery refinements are integrated through
`327fbb09`. Reviewed focus cues, member chrome, group controls, native context
menus and fullscreen restoration are integrated through `d920cdc3`; the reviewed
late-Shift repair follows at `d1bd494e`. Their combined build and runtime gates
are in progress. These integration facts supersede the candidate-only wording
in the earlier observations below, without claiming physical-session acceptance.
Sloom Studio default menu support and the reported ChatGPT launch crash are now
tracked alongside the original completion scope.

The user's first physical session exposed unfinished service recovery,
Settings usability, menu stability, and container-docking behavior. These
reports reopen the earlier deferred polish work; the full concurrent scope is
in [Task list](TASK_LIST.md). No full completion is claimed at this boundary.

Integrated commits `19effa51`, `579332c3`, and `97745214` restore Display cold
activation/retry while preserving its existing recovery journal, Clipboard
session-bus access, Audio/Power cold activation, current Network connectivity
with dense access-point inventories, and more accurate Bluetooth availability.
The complete Debug build passes. Display/Clipboard focused gates pass 4/4;
the broader service selection passes 100/100 after Bluetooth fixture repair
`df090f0f`. The manager independently reviewed that exact fixture candidate and
reran the integrated suite. Strict documentation and link checks pass.

Live user-owned unit overrides now run the verified Display and Network
binaries and the corrected Clipboard unit property without replacing `/usr`.
Audio and Power installed units were started. Typed snapshots verify Display
with one output, readable Clipboard state, Audio and Power Ready, and Network
Ready with Full connectivity after the Network service replacement. These are
observations of the installed session, separate from private test results.
Bluetooth's system daemon still lacks an active service/activation alias on
this host; noninteractive system-service startup was denied. No working
physical Bluetooth adapter is claimed.

QindaPunk palettes, shared editable controls, selected-only segmented actions,
and the default companion wallpaper are integrated through `3de6ac9f`. The full
Debug build passes, the 26 reviewed Controls visual comparisons pass, and the
integrated theme/control/portal selection passes 46/46 after independently
reviewed fixture repairs `fd739656` and `bc87c7c9`. These update stale palette
expectations and isolate the absent-service D-Bus scenario from installed
activation directories. No theme-related failures remain in that selection.

Settings navigation and focused Appearance destinations are integrated through
`fa606016`; the ten focused Settings/Appearance/Customize gates pass, including
real font typing and visible-text restoration after Revert. ICC application is
integrated through `deb48cd3`, with 13 focused Display writer/Color/Clipboard
checks passing. The saved profile is applied through the compositor's public
output-management protocol and reported active only after compositor acceptance.
Global-menu identity churn and replacement-provider failure handling are
integrated through `90bca1c6`; the 43 focused menu/AppShell gates pass.

Grouped-member Meta+Arrow and native quick-tile cancellation are integrated
through `b4453452`, with the plugin build and seven focused input/policy gates
passing. Late Shift added during an already-started native drag remains open.
The private gesture probe failed during window arrangement before reaching its
behavior assertions; no successful nested qualification is claimed for this
change yet. `e0c7ff78` repairs the takeover's drag-source identity (the
adopted window is now KWin's interactive-move owner, not a pointer hit-test
taken after the cancelled move snaps the window back), alongside the
minimized-container resurrection, stale-focus raise, and tab-drag
self-targeting defects; nested live qualification of the full gesture is still
outstanding.

Actual private desktop run `4c6f5c162db04a23adbe5a785af8b300` captured all ten
Settings routes from the refreshed stage. It exposed clipped Customize panels,
weak Appearance layout, duplicate route controls and confusing unavailable-state
copy. These are active repairs, not accepted final visual design. Astra and
Fable 5.1 independently reviewed the actual renders; Fable was invoked through
the Claude CLI and its returned model identity was observed directly. Generated
concepts do not substitute for production screenshots.

The first visual route repair is independently accepted and integrated at
`1250b909`: Appearance uses horizontal tabs, Customize separates its preview
from supporting tools, and Display/Clipboard/Network unavailable presentation
is clarified. The desktop shortcut note is independently accepted and integrated
at `8ce25f98`; its eight integrated focused checks pass. Meta+F1 toggles it and
dismissal persists through Settings. Actual combined screenshot acceptance is
pending; shared typography/status-card refinements remain a follow-up.

Member-title visibility, parent-frame management controls, stronger focus cues,
Gabbee global-shortcut routing, and fullscreen policy remain isolated candidate
or implementation work. The broader suite's seven remaining fixture repairs
were independently reviewed and integrated at `1a68edf7` and `5fd6fb82`; that
checkpoint passed 638/638 broad checks and 3/3 installed Settings routes. The
combined route-plus-note broad run is in progress.

The requested refresh is installed: 1,054 files were verified after atomic
replacement. The shell reloaded from PID719783 to1130466 while compositor719654
and the user's applications remained running. Settings was restarted and the
new Appearance page opened. Display/Network temporary binary overrides were
retired in favor of the new system installation. Audio, Power, Bluetooth and
Network report Ready; Network reports Full connectivity, Bluetooth has one
adapter, Display has one output, and Clipboard history/privacy report healthy.
BlueZ's system service is now active. These are direct physical-session reads.

Private integrated run `e19980545004468a9e86e192f5886c1a` captures all ten Settings
routes and the shortcut note, with byte-exact harness restoration and no private
survivors. Appearance/footer and Customize layout are accepted as an incremental
refresh. Shared status-card/typography and ordinary-window edge refinements
remain open. Compositor candidates have not been loaded into the physical
session; a deliberate restart follows their interaction qualification.

After the refresh the user confirms that global-menu flicker stopped, but menu
clicks are nonfunctional. This is the immediate active defect; the global menu
is not complete. A new isolated Kimi implementation and a production private
click probe are in progress. The source supervisor permits only one unexpected
shell recovery, which this reload consumed: do not send a second shell SIGTERM
and inadvertently terminate the user's compositor session.

The route-plus-note broad run passed638/640, then the two stale Audio/Clipboard
consumer expectations were repaired at `bb5cb9c1`/`8218e5b5` and passed3/3.
The repeat broad run passed639/640; enabling real BlueZ exposed a fixture that
implicitly expected the host system bus to lack Bluetooth. Its private-system-
bus repair is integrated at `ec960670` and passes the integrated focused gate1/1.
No product regression is inferred from that host-dependent test assumption.

## Host installation completed — 2026-09-05

At the user's explicit request, the tested build was installed on the Gentoo
host under `/usr`, including the **QindaQt (Wayland)** SDDM session entry.
It starts `/usr/bin/qindaqt-wm --drm`, which owns the production session.
The full staged payload contained 1,053 files, with no existing destination
files to replace. Post-install verification found no missing files, byte or
symlink mismatches, or files owned by a non-root user. The launcher, supervisor,
shell, notification host, Welcome application, and compositor plugin resolve
their shared-library dependencies. The installed launcher help runs, and the
user service manager configuration was reloaded. Installation used the user's
authorized sudo operation with a graphical askpass helper; no password was
recorded in the project or chat.

To enter the desktop, log out and select **QindaQt (Wayland)** in SDDM's session
selector. Existing sessions remain available. The current desktop was not
terminated. The first physical login remains the hardware qualification step;
the preceding nested verification does not substitute for it. Bluetooth's
system service was inactive during the pre-install audit. The installed-file
manifest and local installation logs are kept in ignored `.cache/host-install`.

## First-launch tutorial and consistent appearance — 2026-09-05

The installed session starts **Welcome to QindaQt**, a seven-chapter illustrated
guide to ordinary windows, arranging splits, desktop tab pages, moving and
detaching members, customization, and appearance. Its **Show at next launch**
checkbox defaults to checked and saves immediately; clearing it suppresses
automatic opening while the launcher entry still opens the guide manually.
The normal 900 × 640 window also supports compact navigation and a fixed footer
at its smaller size. Instructions distinguish desktop pages from application
document tabs, explain divider resizing, and separate canceling a gesture from
discarding a customization draft. See [Welcome](wiki/apps/welcome.md).

One shared appearance policy resolves Light, Dark, System, and an explicitly
selected high-contrast theme. Confirmed changes update the shell and first-party
applications; explicit command-line theme overrides remain authoritative.
Native decoration and grouped-window chrome consume the confirmed palette,
preserving their compact geometry and existing tiling/tab behavior.
[ADR-0080](wiki/adr/0080-resolve-first-party-appearance-from-settings.md) records
the application boundary; [ADR-0081](wiki/adr/0081-project-confirmed-appearance-into-window-chrome.md)
records its process-local chrome adapter.

Ordinary-surface status icons and warning text now use readable semantic
foreground colors. All five built-in themes have checked normal/muted text
contrast, and the 25 changed Controls reference images were independently
reviewed; all 26 ordinary comparison rows pass without relaxed tolerances.

Integrated verification: full Debug build passes on Gentoo; appearance/tutorial
11/11, Hybrid/chrome 21/21, and installed DesktopVirtual/1080p 2/2 pass. The broad
suite passed 651/652; the remaining font-bootstrap structural check rejected
formatter whitespace, was repaired without changing runtime initialization,
and then passed 1/1. Documentation validates 176 pages and builds strictly.
The final profile/display matrix passes 6/6.

Private run `b84c47f82962435081751c6e7ffe23c5` verifies automatic opening,
pointer opt-out, manual launcher reopening, chapter navigation, scrolling,
compact resizing, and use of the compact footer. Its attempted light capture
was rejected because it remained dark. The dedicated follow-up
`74bf6f71dc2641409b5b46680beeef20` asserts Settings activation and the Appearance
route, applies Light, confirms `qinda-light/light` at settings revision 2, and
captures the still-open guide, shell, native Editor, and title bars in Light.
It then applies and confirms `qinda-dark/dark` at revision 4. Both runs exit
successfully, restore their private harness files byte-for-byte, and leave zero
private survivors. Screenshots remain in ignored evidence directories.

Private run `ec1df32e1479300fad55bb845de67c09` passes real pointer splitting,
member detachment, tab grouping and switching, outer resizing, and tab
detachment against the new chrome, with preserved geometry assertions,
screenshots, byte-exact harness restoration, and zero private survivors.
Its visual inspection exposed default-palette controls in Notification Center.
The repaired center, popup, and card buttons now use themed foreground/background
pairs, including checked and disabled states. Focused QML checks pass 4/4;
the rebuilt installed dark 1080p and light WUXGA checks pass 3/3 including the
package fixture. Both actual captures were reviewed for readable header controls.

### Stopping point and first real login

The user requested a bounded stopping point for moving development into the
desktop, not an overnight polishing run. This is a verified nested-desktop
checkpoint; it is not physical DRM/KMS daily-driver qualification. The next
bounded step is installing/registering the existing **QindaQt (Wayland)** login
entry, then exercising the first actual hardware session. Its entry launches
`qindaqt-wm --drm`; starting `qindaqt-shell` alone does not start a complete
desktop. No QindaQt entry was found in the host's `/usr/share/wayland-sessions`
or `/usr/local/share/wayland-sessions` during this handoff, and the current host
session was not replaced. Use the user's requested native sudo password window
for any privileged installation, rather than collecting a password in chat.

Keep physical output/input, suspend/resume, device integration, and the first
hardware login as explicit acceptance work. One known visual follow-up is the
legacy Notifications Settings page, which remains light inside the dark Settings
frame; its text remains readable. Further aesthetic work and memory optimization
are deferred. No background worker or scheduled continuation is required.

## Visual identity and live wallpaper selection — 2026-09-05

QindaQt now ships its own Mineral Light icon theme: 131 canonical designs and
12 explicit aliases, each with colored and symbolic variants (286 SVGs).
Application, action, device, place, MIME, category, and status icons carry
distinct meanings; Wi-Fi signal and battery levels remain distinguishable.
The freedesktop theme inherits hicolor for third-party coverage. Existing
user theme preferences remain authoritative.

Five original wallpapers accompany the theme: Jade Fold, Porcelain Dawn,
Ink Tide, Qinda Punk, and Compile Club. The last two introduce the user's
QindaHumor direction: a chibi cyborg Linux penguin, jade visor, amber boots,
coffee, and a rubber-duck sidekick. The PNG originals are 1672 × 941, with
full generation prompts in `data/wallpapers/ARTWORK.md`. The visual language is
specified in [Visual identity](wiki/shell/visual-identity.md) and
[Icon theme](wiki/shell/icon-theme.md).

Appearance now exposes thumbnails, a readable No wallpaper card, a custom path,
and scale/center/tile choices. Apply updates the actual background through the
existing Settings client. A small wallpaper controller owns one noninteractive
background per output; the exact `desktop` layer-shell scope keeps backgrounds
out of ordinary windows and task lists. Fresh profiles select Jade Fold;
saved choices, including none, outrank that default. Shell, Appearance, and
DesktopVirtual packages include the artwork. [ADR-0078](wiki/adr/0078-own-wallpaper-surfaces-in-the-shell.md)
records ownership and the narrow private LayerShellQt adapter exception.

Verification on Gentoo: full Debug build passes; affected Appearance/shell
preferences 10/10, icon checks 6/6, artwork validation 286 SVGs, and stage-closure
unit checks 5/5 pass. The broad suite initially passed 645/647; both failures
were repaired and their rerun passed 2/2 (a source-format check and an obsolete
icon-theme expectation). DesktopVirtual package and real 1080p checks pass 2/2.

Private run `dc816c3f936bd8357ecb86fbdc5e4c06` succeeds: Settings displays all
five artwork thumbnails, pointer selection and Apply change Jade Fold to
Qinda Punk, and the settings ledger reports `appearance.wallpaper — Applied`.
Captured pixels show the actual penguin background and new dock icons. Both
tracked applications survive and minimize through their title controls;
cleanup leaves zero private survivors and restores the audit hook byte-for-byte.
Screenshots and logs remain in ignored build evidence, not production sources.

This qualifies nested rendering and live selection, not physical output hotplug
or new Release qualification. Third-party application artwork continues to use
its own installed icons and hicolor fallback. Memory optimization stays deferred.

## Dock refinement and menu placement — 2026-09-05

QindaQt and macOS-inspired shelves now use content-sized rounded translucent
surfaces, 40-pixel real icons in 60-pixel tiles, running indicators, hover motion,
and separate pin/task groups. The preserved 72-pixel shelf height leaves usable
padding without restoring the larger panel. Reduced motion disables icon motion;
reduced transparency and high contrast use an opaque surface. Transparent
unused panel margins pass clicks through to underlying application windows.
The QindaQt command bar uses a system menu, leaving one launcher in its shelf.

First-party File Manager, Terminal, Text Editor, and AppShell QML hide their
local menus only after a live global-menu renderer hosts their exact endpoint.
The local fallback returns when hosting fails or disappears; ordinary focus
changes preserve inactive window geometry. Foreign standard registrars retain
local menus. [ADR-0077](wiki/adr/0077-acknowledge-global-menu-hosting-before-hiding-local-menus.md)
records this additive contract. Launcher rows now expose compact, keyboard-focusable Pin/Unpin buttons using
the existing saved pin state. Rows fill the results viewport and keep
application names on one elided line.

Verification on Gentoo system Qt/KWin: full Debug build; dock/panel focused
28/29 followed by the corrected delegate-lookup test passing; broad 640/644
followed by passing repairs/reruns of all four failures (one duplicate stock
launcher, two dock test-fixture errors, and a too-long temporary socket path);
integrated menu 33/35 followed by passing reruns of both repaired registrar
fixtures; desktop-control offscreen 2/2 and launcher offscreen 1/1 after the
new pin path exposed and repaired an icon transform binding and test focus
state. The five profile/display matrix rows plus package fixture pass 6/6.
A final affected-module sweep passes 46/46. Strict documentation and
source-shape gates pass with 13 decomposition warnings. The newly flagged
543-line launcher QML test received independent decomposition review: its
shared fixture and separate behavior slots remain cohesive; revisit splitting
before further growth toward 600 lines.

Private run `ecb6c8693a8e3efe259cd86af2c2b663` passes with screenshots and
asserted Settings-to-Editor focus transition through an unpainted dock margin.
It also captures outward dock tooltips/context menus, task activation, and an
active Editor global menu with no local duplicate. The GNOME-inspired 125%
matrix screenshot retains the Editor's local menu with no global host.
Private run `ff90df2d9b8e1a8a28a24d2fc1a14862` proves primary-click Pin adds
a real Quick Launch tile and Unpin removes it, with full-width rows, readable
names, and both tracked applications surviving clean session shutdown.
Earlier secondary-click probes missed the unexpectedly narrow row hit region;
they do not establish a compositor or TapHandler defect. The final UI uses
direct buttons and the viewport-width repair. Final run
`767246f24aba33ef7a7705ee4f45c223` passes with the corrected 40-pixel pinned
icon and active Editor global menu, no duplicate local menu, and no surviving
private session processes. The audit hook is restored byte-for-byte.

Remaining refinement includes consistent styling of native/context-menu
controls and notification settings, compositor chrome theme propagation, actual
background blur, and richer application grouping/magnification. This slice
implements translucency and modest hover motion, not complete macOS behavior.
Physical-session and Release qualification remain separate; memory optimization
is deferred.

## Usability audit acceptance — 2026-09-05

The selected repair series covers outward Launcher/Bluetooth/Power popups and
Escape, customization drop delivery and applied Discard baselines, saved startup
layout/catalog merging, shell theme/font/accessibility publication, rendered
menu generations, launcher configuration/authentication environment, separate
panel zones and real rows, lazy applet creation, and all stock preset controls.
Additional verified repairs correct launcher settings keys/string-list persistence,
recovery messages, and selected Settings navigation contrast.

The production resolver reports ten profiles, 92 effective instances, all ready
after the dock refinement adds explicit launcher/pin/task groups.
The compact chrome uses one 28-logical-pixel shared title/tab/control row with
right-to-left Mac tabs, plus functional 24-pixel member title strips. The product
build uses Gentoo system Qt/KWin; private nested testing uses the existing
Weston test dependency. No new package installation was needed.

Current verification: full Debug build and shell-runtime checks 6/6; broad sweep 640/643 followed by independent
review and integrated passing rerun of all three repaired harness checks;
installed-service checks 14/14; live shell theme and saved-layout restart;
outward popup/Escape screenshots; compact Hybrid pointer grouping, member
detachment, two-page tabs, visible-page switching, outer resizing and tab
detachment, with state assertions and clean private-session shutdown. The final
profile/display matrix passes 6/6 including its package fixture: WUXGA, 1080p
at 125%, 1440p at 125%, 1080p at 150%, and dual 1080p. The harness unit/syntax
selector passes 3/3. The matrix validator now respects each selected profile;
GNOME overview-only handling is not required to invent a task-list panel.

Remaining scope includes task-list preference delivery, global-menu arrow-key
wrapping, notification control styling, the original audit's additional risks,
and physical-session qualification. Shell preferences do not yet recolor the
compositor-owned shared chrome or native decorations. This series does not claim
Release qualification or complete emulation of the listed desktop environments.
Memory optimization is explicitly deferred; functionality and usability take
priority. Older entries below retain their historical evidence and are not
blanket acceptance for the current tree.

## Current baseline

Manager integration delta after the public baseline below:

- 2026-09-05 — The usability audit repair series supersedes earlier blanket install-readiness claims. The ten selected repair groups are integrated, including compact shared/member chrome. Current acceptance is the system Qt/KWin Debug build on Gentoo with private nested interaction; this series does not inherit earlier Release or physical-session claims. The final audit acceptance record below is the current scope, and remaining audit findings remain open.

- 2026-09-05T02:40:44-06:00 — **Install readiness restored on exact main `3e658510`** (product commit of the polish merge). Integrated the desktop polish `511ac862` at `3e658510` (task icons proven in the nested capture, shell-owned/non-normal windows excluded from tasks, quiet empty chips, Do Not Disturb default); focused 326/326 in Debug and Release, static gates, broad safe Debug 634/634, nested boot/panel-visibility/interactive/CompositorShell1 rows 6/6, installed rows 14/14, session.installpaths 1/1, broad safe Release 634/634 with the /usr/local prefix confirmed. The manager inspected the headless interactive capture: icon chips, real application icons on task buttons, quiet global menu, styled notification center. The install/SDDM smoke requires the user's sudo.
- 2026-09-04T23:37:23-06:00 — **Exact main `6a2019aa` is verified and usable; install is allowed with three known visible polish defects** (task buttons show letter placeholders because the nested stage lacks desktop entries — the real prefix ships them; the shell's own popup is listed as a task; an empty chip renders as a white square) **that the open `desktop-polish` lane (Frances Arnold, OpenAI Codex) is fixing.** Integrated the icon-first panel applets `7eb5372d` at `6a2019aa` (Barbara McClintock ACCEPT `0/0/0/5` after one repair); focused 325/325 in Debug and Release, static gates, broad safe Debug 633/633, nested boot/panel-visibility/interactive/CompositorShell1 rows 6/6, installed rows 14/14, session.installpaths 1/1, broad safe Release 633/633 with the /usr/local prefix confirmed. Since the withdrawal: shell token publication (ADR-0071), iconography I1/I2 (ADR-0072), the atomic task-fact contract (ADR-0073), and the first-party menu export lifecycle are integrated; the manager inspected the headless interactive capture of this tree (icon chips, working task buttons, quiet global menu). The install/SDDM smoke requires the user's sudo.
- 2026-09-04T21:24:52-06:00 — Integrated the first-party global-menu export for Terminal and Text Editor with the fail-closed exporter lifecycle `bfe60099` at `42191636` (Nettie Stevens ACCEPT `0/0/0/0` after three Codex rejections closed successive lifecycle races); focused 190/190 in Debug and Release, static gates, broad safe Debug 630/630, serialized nested boot, panel-visibility and interactive rows 5/5 on the system KWin 6.6.6 roots. `Menu unavailable` no longer appears for first-party windows.
- 2026-09-04T20:37:02-06:00 — Integrated the compositor atomic task-fact contract `fa0e6d9c` at `92d3348b` (Rita Levi-Montalcini ACCEPT `0/0/0/1`; ADR-0073): the task list now lists and controls real windows instead of showing `Limited`; Release focused 202/202 and Debug broad safe 620/620 (which contains the focused rows), static gates, serialized nested boot and panel-visibility rows 4/4 on the system KWin 6.6.6 roots; the boot row now requires the task list ready with real windows.
- 2026-09-04T19:23:40-06:00 — Integrated the shell iconography module I1 `288574a8` at `1207bf39` (Carolyn Bertozzi ACCEPT `0/0/0/2` after one repair; ADR-0072); focused icons/shell-runtime/applet rows 14/14 in Debug and Release and the static gates pass; the broad suite runs with the icon-first applet batch. QQ-004.16 WIRED.
- 2026-09-04T18:42:24-06:00 — Integrated the production shell runtime repair `99de545a` at `cd760f4e` (Tu Youyou ACCEPT `0/0/0/4`): shell and preview publish the QST token facade before panel QML, the boot row validates a `tokens` fact, and the terminal PTY/render startup races are fixed; focused 198/198 rows in Debug and Release, static gates, broad safe Debug 615/615, serialized nested boot and panel-visibility rows 4/4 on the system KWin 6.6.6 roots. Still open before install readiness: the compositor task-fact contract (task list `Limited`), Terminal/Editor menu export (`Menu unavailable`), and the iconography pass; a headless interactive capture of this tree is being taken for visual review.
- 2026-09-04T15:32:08-06:00 — **Install readiness withdrawn.** A windowed run of the exact Release install tree on the host display (private buses, `qindaqt-wm --windowed`) shows an unusable session: the production shell never publishes the `QindaQt.Tokens` facade (only applications bootstrap it), so every QST-based hosted applet renders unstyled (~36k undefined-token warnings, white panels with bare labels); the task list shows `Limited` and the global menu `Menu unavailable` with an exporting terminal active; the terminal's content area stays blank although its bash children run. The green suite never asserted token readiness or live applet state in the production shell. Repair lane `shell-production-runtime-repair` (Ada Yonath, OpenAI Codex) is open from `dd415f48`; do not run the `/usr/local` install until its fix is integrated and a windowed run is visually verified.
- 2026-09-04T13:57:49-06:00 — Exact main `4cafc37c` is Release-install ready: Color route (`32b1ef71`), Task List hosting (`fdd07126` + `bdfef7da`), and Tray hosting (`4cafc37c`) are integrated; on the system KWin 6.6.6 roots the merged tree `4cafc37c` passes 178/178 focused rows in Debug and Release, the static gates, the broad safe Debug suite 614/614, the serialized nested boot and panel-visibility rows 4/4, and the Release install boundary (installed rows 14/14, session.installpaths 1/1, broad safe Release 614/614, /usr/local prefix confirmed). The production dispatcher renders all ten built-ins. Host note: `fs.inotify.max_user_instances` must be ≥1024 (raised live on 2026-09-04; persist in `/etc/sysctl.conf`) or dbus-daemon's inotify warning fails `desktop.virtual.interaction-probe-cli-unit`. The remaining install/SDDM smoke requires the user's sudo.
- 2026-09-04T10:44:07-06:00 — Bluetooth pairing (`e473bbf` at `4b3f07d9`) and Tray S2 (`544d1c3` at `f3abd4ab`, reconciliations `5157a1e0`/`cbaab4e0`) are integrated after their one funded recheck each (both ACCEPT `0/0/0/0`); verified together: focused 105/105 Debug and Release, static gates, broad safe Debug 601/601. New shell-carrying install components go in `src/shell/<Name>RuntimeInstall.cmake`; the closure guard globs those modules. The Color route merge (`0f2bf167`, assistant-verified 62/62) lands next; task-list and tray hosting lanes are live.
- 2026-09-03T13:55:54-06:00 — Exact main `0998b1f4` is Release-install ready: `sys-release` is configured with `CMAKE_INSTALL_PREFIX=/usr/local` and `KDE_INSTALL_USE_QT_SYS_PATHS=OFF`, builds cleanly, passes all 13 installed-label rows, `session.installpaths` 1/1, broad safe Release 592/592, and the strict documentation/source/JSON/Team Board gates. The compiled `qindaqt-wm` paths resolve the plug-in root as `/usr/local/lib64/plugins`; the host XDG data configuration includes `/usr/local/share`, covering the installed D-Bus service directory. The remaining install/SDDM smoke requires the user's sudo authority.
- 2026-09-03T13:49:46-06:00 — Color Settings route `252b2fd` is shelved after Maryna Viazovska's final funded recheck rejected it `0/1/0/0`. Its fresh ICC-root and non-vacuous DesktopVirtual closure repairs pass, but with both buses provably unreachable the real eager Color singleton emits a warning that aborts three fatal-warning Settings host rows in both Debug and Release (52/55). The delegated integration lane was not opened.
- 2026-09-03T13:35:22-06:00 — Task List T2 repair `1e32f0a` is integrated at `431856a5` after Lauren Williams's terminal ACCEPT `0/0/0/0`; merged-count documentation reconciliation `328f8b5e` closes the only broad-guard failure. System-KWin verification passes 33/33 focused rows in both profiles, static gates, the complete 592-row broad-safe Debug set across the corrected rerun, and 8/8 serialized DesktopVirtual rows including both panel scenarios.
- 2026-09-03T13:35:22-06:00 — Tray S2 `b10692c` is shelved after Rózsa Péter's sole funded review rejected it `0/1/0/0`: a malformed live replacement changes registry presentation to degraded without notifying the controller, and no production acknowledgement path exists. Because both applets did not integrate, the named Task List + tray hosting lane was not opened.
- 2026-09-03T13:35:22-06:00 — Bluetooth pairing `7025a1c` is shelved after Kathrin Bringmann's final funded recheck rejected it `0/0/1/0`: duplicate window-context Escape shortcuts in the real Settings host are ambiguous, so an active prompt receives no cancel reply. No further repair lane is funded.
- 2026-09-03T13:03:46-06:00 — QQ-004.02's installed panel-visibility proof is restored on the host system KWin 6.6.6 by accepted repair `1d86b13` at merge `b5c8d87`; manager verification passes focused 125/125 Debug and Release, broad Debug 586/586, and all 8 serialized DesktopVirtual rows including both 1080p and WUXGA panel scenarios, with no live compositor survivor.
- 2026-09-03T12:57:47-06:00 — Program management handed from Claude to Codex; the full state, live lanes, recipes, and the install plan are in `ops/team/messages/manager-results-first/1788461867-claude-program-manager-handoff-to-codex.md`.
- 2026-09-03T12:47:31-06:00 — Session actions integrated at `70763c7`: a user can now lock, log out, suspend, restart, and shut down from the shell; the Wi-Fi secret agent starts with the session.
- 2026-09-03T12:30:19-06:00 — Clipboard applet hosted in the panel (`82e256d`); Wi-Fi join of visible networks (`c269830`). Panel dispatcher count is now eight.
- 2026-09-03T11:21:44-06:00 — Menu export (`a8c171c`), stage closure guard (`0a59236`), secret agent (`287ba6d`), and Clipboard Settings (`22d9f23`) integrated; whole suite green on the system KWin 6.6.6 roots (`sys-dev`/`sys-release`).
- 2026-09-03T10:23:54-06:00 — The KWin plugin ABI pin is now the host's system KWin 6.6.6 (`a5c1c20`, manifest v6.6.6); build with `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake` (system Qt 6.11.1, KF6 6.27, KWin/KDecoration3/PlasmaActivities/KWayland/LayerShellQt 6.6.6; Weston still from the private prefix, confined to the parent compositor's environment). Verified on 6.6.6: compositor 33/33, nested compositor rows 16/16, desktop boot, package contract. Open: both panel-visibility nested rows fail on 6.6.6 (window-overlap-hidden phase) — repair lane open. A `wayland-sessions/qindaqt.desktop` entry is now installed for display managers.
- 2026-09-03T09:48:12-06:00 — Tray S1 transports integrated at `e3eacdd`; new-lane intake paused, in-flight candidates get one funded recheck each.
- 2026-09-03T09:40:00-06:00 — Task list T1 integrated at `90fe400`.
- 2026-09-03T09:33:11-06:00 — Main is nested-qualified again: boot.1080p and both panel-visibility rows green on `ff08ea2` after the staging and capture reconciliations; Clipboard applet C1 and Font F1 integrated.
- 2026-09-03T09:25:01-06:00 — Clipboard applet C1 (`2abc448`), Font F1 (`045f1a1`), and the panel-visibility interaction repair (`ff08ea2`) merged and under batch verification; the DesktopVirtual route staging moved to `tests/session/DesktopSessionRouteStaging.cmake` (`fb2b280`) to restore the source-shape gate.
- 2026-09-03T07:15:39-06:00 — Power Settings route (`a83f34e`) and Display Color C1 (`f6b14b2`, ADR-0066) integrated; any test that constructs the Settings Center's Main.qml in-process must link the static PowerBackend module (`3afa972`).
- 2026-09-03T06:58:16-06:00 — Nested desktop status on main: after the Global Menu and Settings-route merges the private desktop stage lacked libqindaqt_global_menu_qml.so and the Audio/Bluetooth/Power route modules (fixed at `e51372a`, `197f104`; `desktop.virtual.boot.1080p` green again); the two `desktop.virtual.panel-visibility.*` rows still fail on main because the hidden left panel stays mapped at zero size with the Global Menu hosted — an interaction repair lane is open. Treat main's nested panel proof as red until that lands.
- 2026-09-03T06:33:14-06:00 — Text Editor S2 integrated at `28bcd37` (ADR-0065); ADR numbering is assigned at integration when lanes collide.
- 2026-09-03T06:07:45-06:00 — Terminal S2 (search, links) integrated at `9e3422e`.
- 2026-09-03T05:37:29-06:00 — Global Menu G2 (`7de332f`) and File Manager S1 (`0b97954`) integrated; reconciliations `a1bac21` and `4609beb`; the dispatcher now hosts seven built-in applets and every dispatcher-level QML row needs a stub under `tests/shell/qml/imports/QindaQt/Shell/<Module>/` for each module `BuiltinAppletContent.qml` imports.
- 2026-09-03T04:58:12-06:00 — Bluetooth Settings route integrated at `500a33e` (merge `111dedb`); Settings Center order is now appearance, display, network, customize, audio, bluetooth.
- 2026-09-03T04:42:48-06:00 — Controls visual fixtures pinned at `5cf24a2`; the broad safe suite now runs the visual rows again (465 rows).
- 2026-09-03T04:32:48-06:00 — Audio Settings route integrated at `fac8d6a` (merge `f6d47db`); Bluetooth Settings route accepted and merging; Global Menu G2 `f7a49c5`, panel-visibility proof `a52561f`, File Manager S1 `61283bf`, and the controls-fonts repair `b7b5208` are in exact review; Text Editor S2 and Terminal S2 lanes started; GLM is limited until 20:25 and Kimi has recovered.
- This integration merges exact accepted compositor identity descendant
  `8505bdbd09961863538616efd3dd8e7ef834a126` at `58496e8` (ADR renumbered to 0063) and exact accepted
  launcher hosting candidate `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1` at `6f5e213`. The authenticated
  boundary now publishes active-window identity facts to the bound shell owner with an exported, validated
  change signal; the launcher applet renders in the production panels with a closed `LauncherAppletRuntime`
  component. Fresh merged-tree Debug and Release each pass compositor 36/36, window-actions/identity and
  client rows, launcher 17/17, and the integrity/runtime/closure/installed rows under host-unset isolation;
  the sixteen nested KWin rows pass serially; the broad safe Debug suite passes 430/430; 127-document validation, strict MkDocs, source shape,
  diff, JSON, and Team Board 16/16 pass. QQ-004.07 advances WIRED → EXECUTABLE.

- This integration merges exact accepted Terminal S1 descendant `00f2db99ce13df2f426abd93e277de3d33411041`
  at manager merge `00b4f45`. Sessions are bounded, each owns its PTY/child, and shutdown completes only
  when the captured process group is verifiably empty; profiles stay within the existing launch policy;
  Settings1 persistence presents asynchronous apply outcomes accessibly and never replays; argv is
  preserved exactly. Dina St Johnston (OpenAI Codex) accepted the second repair at `0/0/0/0`. Fresh
  merged-tree Debug and Release each pass the terminal selector 15/15 under host-unset isolation; the broad safe Debug suite passes 427/427;
  126-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Launcher L1 descendant `26f366a4a2ab14fc341407f015d21685b8c8415f`
  at manager merge `71900bd`. The scanner, persistence, execution, and compiled applet adapters compose the
  pure L0 model behind injected seams; every adapter test now runs Core-only or offscreen with host display
  and bus variables unset after a reviewer proved the earlier rows attached to the host desktop. Kay
  McNulty (OpenAI Codex) accepted the third repair at `0/0/0/0`. The candidate's ADR is renumbered to
  ADR-0062. Fresh merged-tree Debug and Release each pass launcher 15/15 and applet integrity rows under
  isolation; the broad safe Debug suite passes 421/421; 126-document validation, strict MkDocs, source shape, diff, JSON, and Team Board
  16/16 pass. Production panel hosting of the launcher applet remains a composition lane.

- This integration merges exact accepted Customize canvas descendant `2500a3d71343f238ce30fd0d98f5cc2aa9a95809`
  at manager merge `ce5541e`. The `customize` route composes only the public customization-editor,
  shell-customization, profiles, and Settings1 boundaries into a WYSIWYG panel/applet canvas with
  pointer/keyboard parity, atomic persistence, and discard confirmation on every departure path,
  including window close across the wide/compact host switch; its boundary scan is allow-list-only. Adele
  Goldstine (OpenAI Codex) accepted the third repair at `0/0/0/0`. Fresh merged-tree Debug and Release each
  pass customize 17/17 (route plus domain rows) and Settings Center 9/9; the broad safe Debug suite passes 412/412; 125-document validation, strict
  MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-004.08 advances WIRED → EXECUTABLE.

- This integration merges exact accepted compositor window-actions descendant
  `3690a056e667135d486da4fa60d7996882a4560a` at manager merge `135fe65`. A new authenticated
  `org.qindaqt.CompositorShell1` boundary admits activate/minimize/unminimize/close/raise only for the
  D-Bus caller whose credentials match the bound panel-owning Wayland client, authenticates before any
  parsing, bounds every entry field, replies constant-size to unauthenticated or unbound callers, fences
  by generation, rate-limits, routes Hybrid members through container policy, and ships an exact-owner
  shell client; the unauthenticated `Compositor1` mutators stay `control-disabled`. Margaret Rock (OpenAI
  Codex) accepted the repair at `0/0/0/0`. The candidate's ADR is renumbered to ADR-0061. Fresh merged-tree
  Debug and Release each pass compositor 35/35 non-nested rows and the window-action rows 4/4 including the
  nested live row; the sixteen `compositor.kwin-` rows pass 16/16 serially; the broad safe Debug suite passes 406/406; 124-document validation, strict MkDocs, source shape, diff,
  JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Power PB-2 descendant `6cef8b582aeb33522829d6ae838269f31aad7611`
  at manager merge `4ae7f89`. The resident Power1 service now composes production UPower, logind,
  power-profiles, and backlight adapters over injected buses and roots, defaulting to production with an
  explicit deterministic mode; UPower line-power/PowerSupply semantics, dispatch-time logind
  re-authorization, and generation-fenced pending maps were each forced by an exact rejection. Ida Holz
  (OpenAI Codex) accepted the final descendant at `0/0/0/0`. The candidate's ADR is renumbered to ADR-0060.
  Fresh merged-tree Debug and Release each pass the power selector 26/26 with the host system bus
  unreachable; the broad safe Debug suite passes 403/403; 123-document validation, strict MkDocs, source shape, diff, JSON, and Team Board
  16/16 pass.

- This integration merges exact accepted Audio applet descendant `14f3e670d66988ec87648f195b91e26c56a45fbf`
  at manager merge `780981c`. The applet composes only the public AudioClient with separate read/control
  grants and audited routing; the merge also carries the staging-closure repairs that ship the shell's
  Controls/Tokens runtime dependency in every shell-carrying install component (Power, Bluetooth, Audio,
  and the default component) with a new `qindaqt.shell-runtime-component-closure` row, after the first
  manager merge attempt broke two installed-package rows and was reset. Fresh merged-tree Debug and
  Release each pass the three applet selectors 22/22 and the integrity/runtime/closure rows 7/7; the broad safe Debug suite passes 396/396;
  122-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-004.12
  advances WIRED → EXECUTABLE.

- This integration merges exact accepted Portal P1 candidate `c33b4908f99cb1dfac04287383441d028ab8f25b`
  at manager merge `2c514ac`. The installed `xdg-desktop-portal` 1.20.4 frontend, started on a private bus
  with a staged portal directory, selects the QindaQt Settings backend only under
  `XDG_CURRENT_DESKTOP=qindaqt`, exposes the QindaQt color-scheme/accent/contrast projection and its
  `SettingChanged` propagation, and a Qt offscreen probe under the `xdgdesktopportal` platform theme
  follows it live; non-Settings families route through an explicit fallback table and never resolve to
  QindaQt. Gertrude Blanch (OpenAI Codex) accepted the exact candidate at `0/0/0/0`. The candidate's ADR is
  renumbered to ADR-0059. Fresh merged-tree Debug and Release each pass the portal selector 9/9;
  the broad safe Debug suite passes 391/391; 122-document validation, strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass.

- This integration merges exact accepted Clipboard C1 service repair descendant
  `63e884cfa2216d7dc492407e30c7ce28b8512ac0` at manager merge `f34f81a`. The resident host captures only
  after an explicit user-override `services.clipboardHistory` opt-in (both shipped schemas now default to
  `false`), pauses capture and withdraws readable truth unless the authenticated lock state is Unlocked,
  bounds MIME/byte/offer limits, evicts remembered requests with exactly-once semantics preserved, and
  ships D-Bus activation and a user unit. Evelyn Berezin (Kimi K3-256k) rejected the ancestor at `0/1/1/3`;
  Ruth Lichterman (OpenAI Codex) accepted the repair at `0/0/0/0`. The candidate's ADR is renumbered from
  the colliding 0056 to ADR-0058. Fresh merged-tree Debug and Release each pass clipboard 14/14 and the
  Settings, notification-quieting, and appearance rows 29/29; the broad safe Debug suite passes 389/389; 121-document validation, strict
  MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. QQ-005.06 advances WIRED → EXECUTABLE.

- This integration merges exact independently accepted BlueZ adapter candidate
  `f44919a52f67515779f887b8d54a9bb2a57b3c4b` at manager merge `d43463c`. The adapter implements the
  accepted AdapterBackend port over an injected direct-QtDBus `org.bluez` connection (ObjectManager,
  Adapter1, Device1, PropertiesChanged), calls only Properties.Set(Powered), StartDiscovery,
  StopDiscovery, Connect, and Disconnect, retires truth on BlueZ owner loss, bounds hostile values, and is
  selected by the composition root in production with an explicit deterministic escape hatch. Betty
  Holberton (Kimi K3) accepted the exact candidate at `0/0/0/2`; both P3s (an observed-property
  overstatement and a wording precision) are corrected here, and the candidate's ADR is renumbered from
  the colliding 0056 to ADR-0057. Fresh merged-tree Debug and Release each pass the complete 15/15
  Bluetooth service selector including the whole-repository staged-install row; the broad safe Debug
  suite passes 379/379; 119-document validation, strict MkDocs, source shape, diff, JSON, and Team
  Board 16/16 pass. Physical radios, pairing UX, Settings UI, and hardware qualification remain later.

- This integration merges exact independently accepted Bluetooth applet B1 repair descendant
  `882cc0cdbb31ee9d619c625a2856aee90c7a49b0` at manager merge `34a79c2`. The regex-based positive
  controller-surface gate that four reviewers had split over is replaced by a compiled QMetaObject
  surface test comparing the complete ordered property, method, and enumerator surface and the
  QML-visible names, with token-paste, public-slot, and enum negative controls; the textual
  composition-chain contracts (stock-profile placement, QML delegate wiring, shell composition
  tokens) and dependency-policy poisons are retained. Cecilia Payne (Kimi K2.7) and Chien-Shiung Wu
  (Kimi K3-256k) each accepted the descendant at `0/0/0/0`. Fresh merged-tree Debug and Release each
  pass Bluetooth 8/8 and adjacent 6/6; direct boundary gates pass 7+6 and 5+4; 118-document validation,
  strict MkDocs, source shape, diff, JSON, and Team Board 16/16 pass. The broad safe Debug suite on the merged tree passes 373/373 after a clean incremental build (nested-compositor rows and the 25 host-font-drifted controls visual rows excluded as recorded).
  QQ-004.14 advances from ABSENT to EXECUTABLE; the BlueZ adapter, pairing UX, nested interaction,
  and hardware remain later outcomes.

- This integration merges exact independently accepted Global Menu G1 candidate
  `7c27ee5b1b50746e59f70360d89b0e959328dd47` at manager merge `729bebd`. The registrar owns
  `com.canonical.AppMenu.Registrar` on an injected session-bus connection, keys registrations to the
  caller's exact unique name, and retires them on owner loss; the dbusmenu client decodes `GetLayout`
  replies with bounded depth, item counts, and string lengths, treats property-update signals as
  invalidations followed by a complete revisioned reread, and rejects replayed revisions; the transport
  coordinator binds both to the G0 proof-bound lineage. ADR-0056 records the standard-protocol adoption.
  Elizabeth Feinler (Kimi K3) accepted the exact candidate at `0/0/0/1`; the sole P3 (harness prose claiming
  a lower-revision row that no test drives) is corrected in this integration. Fresh merged-tree Debug and
  Release each pass the complete 16/16 global-menu selector; 117-document validation, strict MkDocs,
  source shape, diff, JSON, and Team Board 16/16 pass. The broad safe Debug suite on the merged tree (all 404 registered rows minus the serialized nested-compositor rows and the 25 controls visual rows that drift only by host font rendering) passes 365/365 after a full 2,715-action incremental build.
  Shell composition, applet wiring, submenu popups, and installed-session proof remain later lanes.

- This integration merges exact independently accepted S3 readiness candidate
  `4d4b3dc395d80135a8f66f5ffcb2395cf1874c60`. The installed private desktop
  executes WUXGA, 1440p at 125%, 1080p at 150%, light/dusk/dark themes, and
  dual outputs while booting the production compositor, shell, resident
  services, Settings, and Text Editor. Its readiness contract joins the exact
  active GlobalAccel component/action, stable unique shell owner and PID,
  private closed/hidden center before one Meta+N batch, open/visible center
  with increased counter after it, and one mapped/committed compositor surface
  on the desired/actual output. Mina Shah's external Claude source/archive
  review accepted the immutable candidate at P0/P1/P2/P3 `0/0/0/2`; Lise
  Meitner's independent fresh static+dynamic review accepted it at `0/0/0/0`
  after building 882/882 actions and passing focused 5/5, formerly failing
  1080p@150% plus package 2/2, and package plus the four-row matrix 5/5. Four
  fresh archives prove canonical activation/shell/surface 4/4, false host
  reachability 12/12, bounded PSS, nontrivial captures, empty teardown, and
  exact dual `[WL-1, WL-0]` authority with interaction and capture on WL-1.
  Fresh merged-tree replay configures successfully, builds 882/882, passes
  focused 5/5, passes the formerly failing 1080p@150% plus package 2/2 in
  8.18 seconds, and passes one unretried package-plus-four-row matrix 5/5 in
  33.94 seconds. Manager run IDs are `666752f4`, `6c6f251d`, `73d82c54`, and
  `93ab1392`; all retain containment 12/12, PSS below the 1,024 MiB ceiling,
  byte-authenticated visually coherent captures, empty teardown, and exact
  WL-1 dual interaction/capture. Final process inspection is empty.
  QQ-004.09 and QQ-006.09 advance from WIRED to EXECUTABLE. Complete
  screen-reader/keyboard coverage, heterogeneous mixed scaling, portrait and
  hotplug/lid behavior, physical input/display, GPU/DRM, and perceptual
  baselines remain later qualification.

- This integration merges exact independently accepted Network Settings N2
  repair `6f5d0ba9915851195a4776b3a1e2f224c369a958`. The installed `network`
  route composes only the public Network1 client into secret-free device,
  access-point, and saved-profile presentation; exact owner, epoch, revision,
  and operation fences withdraw stale truth and make displayed action
  availability identical to request admission. Literal hardware addresses,
  credentials, text-entry surfaces, radio mutation, and private service
  implementation remain outside the route. Barbara Liskov's external Claude
  exact rereview accepted the repaired descendant with P0/P1/P2/P3
  `0/0/0/3` after directly closing all four former P2 findings. Fresh merged-
  tree Debug and Release each build **2,036/2,036** and pass the mutation 5/5,
  affected 14/14, public package/policy 5/5, Network 25/25, and Settings 9/9
  selectors. Documentation validates 116 pages; strict MkDocs, source shape,
  JSON, diff, conflict, direct boundary, and poison gates pass. QQ-006.05
  remains WIRED because other platform-service Settings routes and whole-
  desktop qualification remain.

- This integration merges exact independently accepted Portal P0 repair
  `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`. The resident backend owns only
  the standard `org.freedesktop.impl.portal.Settings` endpoint, projects
  complete exact-owner Settings1 appearance truth through QST-1 into the
  standard color-scheme, accent-color, and contrast values, and withdraws
  readable truth on owner, epoch, bus, or projection loss. Source and staged
  package metadata are exact Settings-only singletons; OpenURI, installed
  xdg-desktop-portal 1.20.4 Background, duplicate Settings, extra families,
  and private headers fail closed. Frances Allen's exact rereview accepted the
  repaired descendant with P0/P1/P2/P3 `0/0/0/0`; fresh Debug and Release
  each built 97/97 and passed the contained seven-row selector. The manager
  merge resolved only additive ADR/navigation conflicts and independently
  repeats strict Debug and Release **97/97 plus 7/7** each, 114-document
  validation, strict MkDocs, the 1,715-file source-shape gate, JSON, diff, and
  conflict checks. QQ-005.09 advances from ABSENT to EXECUTABLE. Host portal
  selection, toolkit reaction, every non-Settings portal family, and physical
  distribution qualification remain explicitly outside P0.

- This integration merges exact independently accepted notification live-output
  repair `89557a0a090b6b910621463b4ac97a6d1d054469`. The production shell now
  binds the exact current `org.qindaqt.Compositor` owner, consumes the ordered
  public `Compositor1.Outputs()` projection, and joins its generation and exact
  output-ID set to accepted shell-visibility and Qt inventory truth before
  resolving notification popup/center surfaces. Owner loss, invalidation,
  malformed replies, replacement, generation mismatch, and missing exact Qt
  screens fail closed; stale `QGuiApplication::primaryScreen()` no longer
  selects notification output. Charles Babbage's immutable review accepted the
  candidate with P0/P1/P2/P3 `0/0/0/1`: Debug built 297/297 plus 161/161
  adjacent actions, passed 20/20 and repeated both focused rows 25 times;
  Release built 458/458 and passed 20/20. The sole documentation-precision P3
  is corrected in this manager integration. After an initial Debug attempt
  stopped only because the `/tmp` filesystem filled, exact build-system clean
  targets reclaimed reproducible old outputs and the fresh merged tree passed
  Debug and Release **458/458 plus 20/20** each. QQ-004.09 remains WIRED until
  the preserved S3 worktree proves the real WL-0 to WL-1 layer-surface transfer
  and completes the wider whole-shell matrix.

- This integration merges exact independently accepted Display D6 repair
  descendant `9a7872aec60a5e0f8286b3d5af7fa21209e8fd65`. The packaged Display1
  process now resolves the injected durable journal before mutation authority,
  authenticates its compositor Wayland peer and lock/logind safety inputs,
  and composes the public D4 writer through the resident D2 service. Typed
  observation disposition preserves complete live truth and every Staged-or-
  later transaction across benign same-owner rejection while still publishing
  unavailability after a failed replacement-owner establishment. Mary
  Jackson's same-reviewer rereview accepted the exact descendant with P0/P1
  `0/0`; Debug and Release each passed 19/19 hostile service assertions, 7/7
  D6 package/boundary rows, and 40/40 adjacent D0-D6/session-lock rows. The
  prior in-tree poison and WaylandClient configure defects are closed. Fresh
  merged-tree Debug and Release targeted builds complete 197/197 and 228/228
  actions respectively; each passes the same 40/40 adjacent, 7/7 focused, and
  direct 19/19 hostile gates. The review's unrelated
  customization Release P2 is already repaired by integrated `fa22af50`.
  Nested compositor convergence, mixed/physical output behavior, resources,
  suspend/hotplug, and hardware qualification remain later work.

- This integration merges exact independently accepted customization-editor
  Release-portability candidate `fa22af5028e8dd4bfd9c0951cf742ea74d4b914f`.
  The panel-step helper now constructs the complete `DropTarget` value and
  copies it into the outer optional, removing GCC 15.3's optimization-only
  inactive-storage diagnostic without suppressing warnings or changing the
  panel, zone, or null-anchor contract. Grace Hopper's exact review reproduced
  the parent failure at action 59/85 and found P0/P1/P2/P3 `0/0/0/0`; strict
  Debug and Release each built 85/85 and passed the complete 6/6
  customization-editor selector plus the repaired direct row 3/3. Fresh
  merged-tree verification repeats those affected gates. This portability
  repair does not advance QQ-004.08 beyond WIRED; the Settings canvas,
  provisional live-shell binding, reveal UI, rendered matrix, and installed
  session behavior remain later work.

- This integration merges exact independently accepted Network N1 repair
  descendant `aebc4fd3d887f09ae28149f9c016a08f28c86a92`. The resident Network1
  service now composes the N0 model/client with a public Qt transport and a
  libnm-confined NetworkManager adapter. Exact upstream-owner notifications
  retire old generations beneath the fact-refresh interval; admitted backend
  work is queued until the delayed bus reply is owned; definite scan failure
  removes provisional freshness while uncertain cancellation remains
  conservative. Katherine Johnson's exact rereview found P0/P1/P2/P3
  `0/0/0/0`, passed strict Debug and Release 21/21 each, repeated four
  mutation-sensitive rows ten times each for 40/40, and passed the positive
  boundary, six-poison negative, and installed lifecycle selector 3/3. Fresh
  merged-tree Debug and Release each build 111/111 focused actions and pass
  the complete 21/21 Network selector with host display and bus variables
  removed. Physical radios, UI, persistence, external secret-agent/credential
  interaction, distribution policy, and hardware qualification remain later
  gates.

- This integration merges exact independently accepted Power P2 descendant
  `9e4b7a60f2bcc9c9229418a47c2c1801a343a876`. The production panel applet
  composes only the public PB-1 client through a shell-private controller,
  requests independent `power.read` and `power.control` grants, clears stale
  truth and pending operations on exact-owner replacement, exposes compiled
  keyboard-accessible QML, and is discovered through the audited manifest,
  registry, host, profile, and installed-package seams. Ada Lovelace's exact
  descendant rereview found P0/P1/P2/P3 `0/0/0/0`; Debug and Release each
  passed the six applet-integrity rows, eight Power rows, and 11 direct manifest
  cases. Fresh merged-tree Debug and Release each build 327/327 focused
  actions and pass the combined 14/14 selector plus 11/11 direct manifest
  cases. QQ-004.13 advances from WIRED to EXECUTABLE. PB-1 still reports
  honest `upstream-not-integrated` truth; live providers, successful host
  mutations, nested interaction, and physical hardware remain later gates.

- The manager merge `7277771a63747bbcec957465e5f0b676e69168d0`
  integrates exact accepted Display Settings descendant `2f429c11`. The live
  `display` route composes the public D3 client/coordinator into bounded
  snapshot, draft, topology-validation, preview, confirm, and revert behavior;
  preserves authoritative coordinate truth across output replacement and
  same-output refresh; and exposes keyboard-accessible output selection and
  integer coordinate commits through the installed Settings application.
  Katherine's terminal rereview found P0/P1/P2/P3 `0/0/0/0` after the sole
  documentation-integrity defect was repaired. Exact Debug and Release review
  each built 328/328 focused actions and passed 12/12 rows, the compiled page
  passed 10/10, the independent interaction probe passed 7/7, and four hostile
  mutations were killed. Fresh merged-tree verification builds 328/328 and
  passes the same combined 12/12 selector. Display resident composition is now
  integrated by D6; nested preview/confirm/revert convergence remains S3 work; physical
  displays and assistive-technology integration remain release gates.

- The manager merge of exact accepted candidate `acd0168` integrates Display
  D5's crash-safe filesystem journal behind the D4 `JournalStore` boundary.
  The injected effective-user-owned state root, fixed names, canonical bounded
  codec, mode-0600 exclusive temporary file, file sync, atomic replacement,
  and directory barrier preserve one complete recovery pre-image without
  hidden HOME/XDG authority. A typed `DurabilityUncertain` outcome prevents a
  visible rename or unlink from authorizing forward apply before the directory
  barrier is proven; even an already-absent clear retries that barrier and the
  D1 machine remains cleanup-only `Stuck` with zero apply requests until a
  concrete durable clear. Galileo's terminal exact rereview found P0/P1/P2/P3
  `0/0/0/0` and passed strict Debug and Release journal/writer/transaction
  12/12, direct lifecycle 4/4, adjacent service/client 10/10, package poison,
  docs, strict MkDocs, shape, lineage, provenance, and residue gates. Manager
  replay builds all 130 focused actions and passes the 12/12 and adjacent 5/5
  selectors serially. Resident startup recovery/writer composition,
  authenticated lock/logind safety, nested convergence, mixed
  outputs, resources, and physical hardware remain D6+.

- `26bb7f5` supplies the exact independently accepted private interactive
  desktop S2 replay. Astra's immutable Gemini review found P0/P1/P2/P3
  `0/0/0/0`, completed a fresh 2,338-action build, passed 73/73 desktop-session
  units and both private boot/interaction rows, and preserved the candidate
  byte-clean. The combined D4+S2 manager tree completes 2,201/2,201 build
  actions and passes package, boot, and interactive 3/3 plus 73/73 units. Its
  private run `831b6c817364cd4765468fa3194f0d96` observes zero active centers
  before exact `Meta+N`, then a mapped 440x640 center; captures a 1920x1080
  parent frame with 77 full-frame and 48 bound-region colors; measures 167,633
  KiB across all eight QindaQt roles under the 1,048,576 KiB ceiling; and
  records eleven authenticated terminal phases with zero survivors. This
  advances QQ-006.09 from MODELLED to WIRED without claiming the wider DPI,
  theme, multi-output, screen-reader, GPU, or physical-device matrix.

- `d7691ac` integrates the exact independently accepted Display D4 public
  QtWayland output-management writer. Galileo's immutable review found
  P0/P1/P2/P3 `0/0/0/0`, verified both lifecycle repairs and both decomposition/
  mutation-coverage repairs, and passed Release D4 5/5, Debug D0-D4 26/26,
  installed package poison, exact protocol hashes, docs, strict MkDocs, shape,
  provenance, cleanliness, and zero residue. Fresh manager-tree Debug
  verification built all 23 executable Display targets and passed the complete
  D0-D4 selector 26/26. The packaged resident remains deliberately fail-closed
  until authenticated lock/logind safety, writer/journal resident composition,
  and contained nested convergence land.

- `c819db8` integrates the exact independently accepted Display D3 typed
  asynchronous client and D2 transaction-summary projection replay. Astra's
  immutable Gemini review found P0/P1/P2/P3 `0/0/0/0`; the replay preserves
  all 20 D3 leaf blobs and seven D2 source/test blobs while retaining every
  current-manager shared-registry entry. Fresh manager-tree strict Debug and
  Release builds complete 81/81 targeted actions and pass the exact seven-row
  D2/D3 selector in each profile. D4 now supplies the separately integrated
  compositor writer, D5 supplies the durable journal, and the Display Settings
  route is now integrated; resident composition, nested convergence, hardware,
  and resource proof remain.

- `0c9f4b0` integrates the exact independently accepted Settings Center S1
  repair over typed navigation commit `80a91f8`. Noether's immutable rereview
  found P0/P1/P2/P3 `0/0/0/0`; Debug and Release passed 9/9, the direct page
  binary passed 6/6 under `QT_FATAL_WARNINGS=1`, the external responsive focus
  harness passed 5/5, and package-isolation poison, docs, source shape, strict
  MkDocs, provenance, and cleanliness passed. Fresh manager-tree focused
  build and the exact 9-row selector plus direct 6/6 pass. QQ-006.04 advances
  WIRED to EXECUTABLE; most platform pages, drag-from-configuration editing,
  cross-app visual matrices, and live assistive-technology proof remain.

- `2ae29f3` integrates the exact independently accepted Display Color C0
  series. The GLM repair defeated all eight hostile review reproductions;
  independent Gemini Pro review found P0/P1/P2/P3 `0/0/0/0`, completed strict
  Debug and Release builds, passed 6/6 registered rows and 46/46 direct cases,
  and preserved the candidate tree exactly. The fresh manager tree builds all
  1,597/1,597 actions, passes 6/6 registered rows and 46/46 direct cases, and
  passes documentation, source-shape, strict MkDocs, and diff gates. QQ-005.07 advances
  ABSENT to EXECUTABLE. C0 remains a pure injected model: live ICC discovery
  and import, persistent assignment, compositor application, Settings UI,
  nested HDR/WCG proof, and physical hardware qualification remain later.

- `ea4d986` replays the exact independently accepted Network N0 series onto
  the Terminal and Bluetooth manager tree with additive shared registries.
  Independent exact review found P0/P1/P2/P3 `0/0/0/0`, proved all 49 Network
  leaf blobs byte-identical and all seven shared paths additions-only, and
  passed Debug/Release 13/13, direct 118/118, eight mutation checks, package,
  poison, docs, and provenance gates. Fresh manager evidence passes 64/64
  focused build actions, 13/13 registered rows, source shape over 1,477 files,
  99-page docs, and strict MkDocs. QQ-005.04 advances ABSENT to EXECUTABLE;
  resident service, NetworkManager/secret transport, persistence, UI, radio
  mutation, and hardware qualification remain N1+.

- `c08b32e` replays the exact independently accepted Bluetooth B0 series onto
  the Terminal milestone with additive Terminal/Bluetooth ADR, module, source,
  test, and documentation registries. Independent exact replay review found
  P0/P1/P2/P3 `0/0/0/0`, proved 54/54 Bluetooth blobs byte-identical, and
  passed 9/9 rows, 70/70 direct assertions, the staged package and its poison
  negative. Fresh manager evidence passes all eight source/private-bus rows,
  source shape over 1,431 files, 96-page docs, strict MkDocs, and Team Board
  17/17. QQ-005.05 advances ABSENT to EXECUTABLE; production BlueZ, hardware,
  pairing UX, Bluetooth audio, suspend/hotplug, resource, and UI remain later.

- `4f99a7f` replays the exact independently accepted Terminal S0 series and
  its relocatable-qtermwidget package repair onto the current manager tree.
  Independent exact review found P0/P1/P2/P3 `0/0/0/0`; all 20 production
  Terminal blobs match the private-Weston-qualified candidate. Fresh manager
  evidence passes 63/63 build actions, 9/9 registered rows, 7/7 appearance
  cases, 4/4 real-adapter cases, source shape over 1,383 files, 93-page docs,
  and strict MkDocs. QQ-006.08 advances from ABSENT to EXECUTABLE; S0 does not
  claim tabs/profiles, settings persistence, AppShell/global-menu integration,
  a nested screenshot matrix, or whole-application assistive-technology proof.

- `d0e0809` replays the exact independently accepted Text Editor AppShell
  migration `75f786e9`. The manager tree builds the seven focused editor
  targets in 139/139 actions and passes the Text Editor selector 10/10 plus the
  rebuilt AppShell/File Manager/Appearance adjacent selector 17/17. Source
  shape checks 1,354 files, documentation validates 90 pages, strict MkDocs
  and all 16 Team Board tests pass, and only the manager-owned provider-status
  record remains operationally modified. The later localization/global-menu
  authority is a nonblocking P3; no portal or live desktop claim is added.

- Branch: public `main`
- Functional boundary: public milestone `ab36cd8` plus exact accepted
  Appearance Settings S0 candidate `d71fac4` and the privately qualified
  1920x1080 whole-desktop boot boundary in this integration change
- Outcome: qualified QST-1 and Controls, bounded Audio1, Display D0/D1/D2,
  live Notifications and Appearance settings routes, executable native Text
  Editor S1 and local File Manager S0, and executable shared
  QindaQt.AppShell 1.0 contracts, plus a production-built private whole-desktop
  boot with exact topology, a 1024 MiB PSS ceiling, and bounded teardown
- State: independently accepted, manager-qualified, and published with a
  documentation-only project-identity descendant

The baseline combines generic persistent Settings1 and the first-class
Notifications route with QST-1's pure semantic token derivation, accessibility
overrides, read-only QML adapter, and installed consumer packages. The
Settings1 resident exits on permanent session-bus loss; a new daemon activates
a new process and lineage rather than reconnecting stale repository state.
QST-1 owns semantic policy without importing a general application framework
or widening the theme schema. Audio1 adds a versioned, asynchronous Qt
boundary over a resident service whose production WirePlumber and GLib handles
remain confined to one private worker thread. Run generations, owner/epoch/
revision lineage, and atomic validation prevent stale or malformed backend
state from reaching future shell and Settings consumers. Display D0/D1 adds a
revisioned compositor inventory plus bounded protocol, identity, topology, and
reversible transaction state. Text Editor S1 adds the first native application:
one local UTF-8 document with optimistic conflict checks and atomic persistence.
The installed Notification Live path qualifies the shell shortcut,
keyboard/focus behavior, Settings1 persistence and replacement, Do Not Disturb,
critical bypass, shell restart, authenticated private lock privacy, and bounded
teardown across the required nested resolution and scale matrix.

Integrated evidence:

- The combined production graph built 612/612 targets and the dependency-light
  integrated suite passed 189/189. The private `desktop.virtual.boot.1080p`
  row then passed with one `Virtual-0` 1920x1080@1 output, exact compositor,
  Settings1, Audio1, and Notifications owners, mapped Settings and Text Editor
  windows, the supervised shell/session process topology, zero teardown
  survivors, and 88,688 KiB resident PSS against the 1,048,576 KiB ceiling.
  Cold-boot polling now keeps each probe inside its fixed one-second lifetime:
  service gaps produce retryable complete snapshots under the outer 15-second
  budget instead of allowing an inner wait to self-timeout before evidence.
- The exact Appearance Settings repair `d71fac4` passed independent rereview
  with P0/P1/P2/P3 `0/0/0/1`. Its six-target warning-clean build and direct
  suites passed 7/7 values, 8/8 preview, 11/11 plus 6/6 adversarial model,
  9/9 page, and 10/10 migration checks; registered selectors passed 4/4
  Appearance, 5/5 Settings application/package, and 1/1 migration rows. The
  manager's combined tree repeated those registered rows, retained both
  Notifications and Appearance installed routes, and made `DesktopVirtual`
  stage the Appearance, Tokens, and Controls transitive runtime instead of
  publishing an incomplete Settings package. The remaining P3 is later live
  assistive-technology/nested visual qualification; no host desktop or input
  was contacted.
- The exact File Manager runtime/package repair `3fd3842` passed independent
  rereview with P0/P1/P2/P3 `0/0/0/1`: fresh strict serial build 138/138,
  focused File Manager selector 8/8, hostile parent failure on all three
  repaired seams, real staged `Loading` to `Ready`, bounded timeout/error and
  nested-loop lifetime probes, exact relative RUNPATH, confined QML/library
  inventory, strict docs, source shape, and clean provenance. The manager's
  combined tree separately builds the five focused targets and passes the same
  8/8 selector plus all 53/53 File Manager, QST/Controls, AppShell, and
  Power/Brightness rows after a complete installable-tree build. Strict
  72-page documentation and the 1,098-file source-shape gate also pass. The
  remaining P3 is a
  direct repository-owned timeout/Error unit row; the installed runtime row is
  already non-vacuous and the authority remains read-only/local.
- The exact repaired contained-virtual-desktop candidate `d08747d` passed an
  independent exact-commit rereview with P0/P1/P2/P3 `0/0/0/0`. On the
  combined tree, all 62/62 focused Python units pass, 14 harness sources
  compile in memory, source shape checks 1,069 files, documentation validates
  68 pages, strict MkDocs passes, and the staged diff is whitespace-clean.
  This integrates the authenticated sandbox, package contract, topology,
  resource, evidence, and teardown boundary only. No compiler, private desktop
  boot, screenshot, input, or host-session action ran in this merge.
- The exact PB-0 candidate `3078386` passed independent GLM rereview with no
  P0/P1/P2 finding. The combined tree built all five focused test targets,
  passed 6/6 Power/Brightness CTest rows and 54/54 direct QtTest cases, and
  retained 5/5 Display1 and 5/5 AppShell regressions after their binaries were
  built. Documentation navigation, strict MkDocs, source shape, and whitespace
  also pass. PB-0 remains a pure WIRED boundary, not a resident service or UI.
- The exact PB-1 collision-recovery descendant
  `a8a57a9856666c6293fac6872c27c0be9928d8c4` passed Noether the 5th's
  immutable rereview with P0/P1/P2/P3 `0/0/0/0`. The manager merged it onto
  the D4+S2 tree without conflict, completed 139 incremental build actions,
  and passed the exact client/service/package/private-lifecycle selector 8/8.
  Reviewer probes prove valid profile/session siblings recover both when a
  colliding battery identity is replaced and when battery facts become
  unavailable, while intrinsically malformed profiles never resurrect. PB-1
  is an EXECUTABLE resident injected/unavailable boundary; production UPower,
  logind, profile and brightness adapters, policy persistence, Settings/shell
  UI, physical hardware and suspend/hotplug proof remain later work.
- The exact repaired AppShell candidate `5c914a6` passed independent GLM
  rereview with no blocking finding. The combined tree then built the five
  AppShell targets serially and passed 5/5 action-registry, coordinator,
  offscreen accessibility/close-consent, source-policy, and clean installed-
  consumer rows, plus documentation navigation, strict MkDocs, source shape,
  and whitespace checks.
- The immutable Notification Live candidate passed an independent five-profile
  private nested matrix and ten repeated 1080p lifecycles. The conflict-resolved
  manager commit then passed an independent exact-tree integration review, a
  fresh 1,299-action combined Debug build, 11/11 exact focused regressions, and
  a fresh installed private 1080p smoke. No matching private process or recent
  fixture root remained afterward.
- The exact Text Editor candidate passed independent review, then built in the
  integrated Debug tree and passed all 8/8 focused document, store, controller,
  large-document, offscreen window, desktop metadata, CLI, and installed-theme
  tests. Its accepted candidate also passed Release/package proof and measured
  266 ms startup with 19,511 KiB median PSS.
- The accepted Audio candidate and the exact integrated functional tree both
  received different-worker review with P1/P2/P3 `0/0/0`.
- Fresh strict-warning Debug and Release builds passed 749/749 steps each.
  The focused Audio selector passed 7/7 and the complete QindaQt registry
  passed 108/108 in both configurations.
- Debug and Release activation/runtime/reset lifecycle stress passed all three
  tests for ten repetitions each: 30 executions per configuration, 60 total.
- A fresh ASan+UBSan build passed 59/59 focused steps and all 7/7 Audio tests
  with leak detection and halt-on-error enabled, including the 250-cycle
  worker teardown and deterministic reset-source barriers.
- A fresh testing-disabled production/package build passed 485/485 steps and
  all four QML-lint targets. Its 186-file staged install contains the exact
  Audio executable, public libraries/headers, D-Bus descriptor and XML, and
  hardened systemd user unit with staged executable resolution.
- The exact installed Audio descriptor completed 10/10 private-D-Bus daemon-
  loss/replacement cycles: 20/20 staged service activations and exact PID exits,
  10/10 distinct owner/PID/epoch replacements, zero surviving staged services,
  and zero fixture roots.
- Documentation link/navigation validation, source-shape audit, strict MkDocs,
  whitespace, and post-test process cleanup passed on the integrated tree.
- No active desktop, user session bus, global input, host audio graph/device,
  physical display, or physical screen lock was touched by this evidence.

## Next outcome

Extend the accepted private interactive desktop S2 evidence described in
[Task list](TASK_LIST.md) across WUXGA, 1440p, representative 125%/150% scales,
light/dusk/dark themes, and a real multi-output arrangement. Preserve the exact
private-seat input, machine-bound screenshot region, all-eight-role 1,024 MiB
ceiling, and authenticated zero-survivor teardown without connecting to the
host pointer, display, session bus, or user configuration.

The reusable `QindaQt.Controls 1.0` component set is now integrated after exact
independent Debug/Release, visual, accessibility-event, package, source-policy,
and PSS qualification. The revisioned compositor output inventory and contained
virtual-output development seam are also integrated after exact review and
focused integrated-tree verification. The pure Display1 protocol, identity,
topology, and reversible transaction model are now integrated after the
same-revision lineage defect was reproduced, repaired, and exactly rereviewed.
The resident Display1 service and exact-owner compositor inventory adapter are
now integrated at `a5528f8` after the A/B/A epoch-reuse defect was repaired and
two independent exact reviews passed. A fresh combined-tree Debug build passed
68/68 focused build steps and all five Display1 service tests, including both
serial private-D-Bus lifecycle rows, with no surviving service or fixture.
Display1 now exposes the fail-closed D3 typed asynchronous client and
server-projected reversible transaction coordinator; D4, D5, and the Display
Settings route are integrated separately. Production output mutation remains
unavailable until resident writer/journal composition and contained nested
preview/confirm/revert convergence proof land.
Power/Brightness PB-1 is integrated as an EXECUTABLE resident service/client,
package and private lifecycle boundary over the PB-0 protocol/aggregation/
brightness foundation. PB-2 production upstream adapters and policy remain
behind the routed session-lane activation contract. A source-only handoff or a
live worker process is not completion.
