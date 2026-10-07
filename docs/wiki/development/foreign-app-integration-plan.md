# Android and Windows applications as desktop applications

> Implementation was separately authorized on October 7 after this audit.
> The repository task list (`docs/TASK_LIST.md`) and [delivery queues](../contributing/flow-team-workflow.md#durable-queue-contract)
> record current dispatch and evidence. The proposals below remain the planning
> baseline; assignments alone do not establish delivered behavior.

The owner's October 7 requirement is a native-feeling application experience:
Android applications run in resizable windows and appear in Applications with
a **green highlight**; Windows applications through Wine/Proton use a **blue
highlight**. Otherwise they participate in the desktop like ordinary apps.
This is part of the [everyday desktop plan](everyday-desktop-plan.md), not an
implemented feature or an assertion of universal application compatibility.

## Existing foundation and remaining work

The audited laptop has Waydroid `1.6.3`, Wine Proton `11.0.2`, GE-Proton `11.6`
and Lutris `0.5.22-r1` package records. QindaLutris has an installed desktop
entry. Presence does not prove that an Android image is initialized, an app
runs, a GPU path works or a window resizes; this pass did not launch or change
those runtimes.

Waydroid already supports an application-menu launch path and a multi-window
property. Therefore the first feasibility target is to use its runtime behind
a better integrated QindaQt experience, rather than write Android from scratch.
The user's requested improvement is the complete desktop workflow; choosing
Waydroid as an implementation component does not satisfy that workflow by
itself. [Waydroid applications](https://docs.waydro.id/usage/install-and-run-android-applications),
[window-integration properties](https://docs.waydro.id/usage/waydroid-prop-options).

QindaLutris already owns Windows launch records, prefixes, setup jobs and pinned
Proton builds. See [QindaLutris](../apps/qindalutris.md) and
[installation jobs](../apps/qindalutris-installs.md). Extend or extract an
explicit public runner boundary; do not make the shell import game-library
internals. Desktop productivity apps should not have to masquerade as games.
Use Wine or a tested per-app Proton recipe as appropriate; Proton's upstream
purpose is Windows games through Steam Play, not a guarantee for all Windows
software. [Valve Proton](https://github.com/ValveSoftware/Proton).

The shared application catalog currently feeds both launcher and File Manager
Applications. `src/shell/launcher/.../launcher_types.h` and desktop-entry parsing
carry desktop ID and StartupWMClass; `src/workspaces_apps/desktop_applications.cpp`
resolves known desktop IDs and unique aliases. The inspected boundary does not
establish an Android/Windows provenance type or a complete trusted
runtime-app-to-window association. Those are design work, not merely icon paint.

## Product behavior

| Surface or action | Required experience |
| --- | --- |
| Applications menu, launcher search and File Manager Applications | One entry per launchable app, with its real name and icon, ordinary category/search/pinning behavior, and Android green or Windows blue identification. No duplicate generic “Wine”/“Waydroid” entries standing in for every app. Optional platform filters supplement the combined list. |
| Running window, dock, task switcher and container tab | Resolve the same app identity. Each app has an ordinary title, icon, focus, minimize/maximize/close and usable resize behavior. Android green and Windows blue accents stay consistent across relevant app-identity surfaces. |
| Grouping and multiple windows | Join/detach tabs and splits with native apps. One app may own several windows/dialogs; child prompts stay with the correct app and app close must not terminate other apps sharing a runtime. |
| Appearance and accessibility | Use a restrained green/blue icon badge, edge or underline through semantic theme tokens. Also expose “Android application” or “Windows application” in tooltip/details and accessible description. Preserve focus/selection/error colors; origin is not conveyed by color alone. Test dark/light/high-contrast and color-vision cases. |
| Launch and failure | Start the necessary runtime on demand with visible progress, bounded timeout and plain recovery. An unavailable runtime, unsupported architecture or failed app does not leave a permanent spinner or false running icon. |
| Files and links | Open supported file types/URLs through chosen defaults and bounded host/guest path mapping; provide explicit file import/export and recoverable errors. No requirement to locate an Android container path or Wine drive mapping manually. |
| Input and media | Keyboard shortcuts, mouse, touch, IME, clipboard, audio and scale behave predictably. Integrate notifications and portal-style permissions where the runtime can support them, with explicit limits where it cannot. |
| Management | App details show origin, runtime/prefix, version and compatibility status. Provide graphical install/remove and runtime settings; keep app data on uninstall unless deletion is explicitly selected. |
| Terminal and agents | Stable launch/list/status identifiers and structured outcomes through a public adapter. Origin and current capabilities are available to agents; native-looking windows do not imply a semantic document provider exists. |

Green/blue denotes execution origin, not trust or permission. An arbitrary
application cannot gain stronger authority by supplying a desktop key, title
or WM_CLASS. Preserve desktop-entry overrides while authenticating runtime
associations separately. Mixed containers retain origin per member instead of
painting the whole container as one platform.

## Proposed architecture work, before code

ED-20 must produce a reviewed ADR for three boundaries: application registration
and provenance, runtime lifecycle/launch, and window identity. The shell owns
presentation and window behavior; a runtime adapter owns Android lifecycle or
Wine/Proton launch; the app catalog owns stable registration and deduplication.
Adapters exchange bounded typed records, not shell command strings or private
configuration. A runtime restart cannot silently bind an old app handle to a
new unrelated window.

Start by proving Waydroid's actual multi-window behavior on the native
compositor. If essential window isolation or resizing cannot be made reliable,
record the reproducer and compare a bounded runtime alternative in the ADR.
Do not start a runtime fork before identifying the missing contract. Some
Android apps assume fixed orientation or dimensions: the supported fallback is
a resizable outer window with an honestly constrained/letterboxed app surface,
not distorted coordinates or a claim that the app itself supports free resize.

Installation must honor the owner's **Portage-only software policy**. Package
runtime binaries, services, integration helpers and Android images through
Portage. ED-20 also specifies a Portage-tracked guest-app recipe/manifest and
graphical installer adapter for APK/MSI/EXE payloads, leaving per-user data in
the runtime's user storage. App-store updates and existing QindaLutris direct
download behavior need explicit reconciliation with that policy before adoption;
this plan does not silently create a separate unmanaged software installer.
Do not bundle proprietary images/stores or promise their services as the
default. Start with appropriately licensed test apps and record exact artifacts.

## Bounded work packets

All rows are proposed/unclaimed; no candidate, reviewer, worktree or active
coding assignment exists. Dispatch uses a fresh exact hub base as specified in
the [main plan](everyday-desktop-plan.md#proposed-work-packets).

| Packet | Owner and dependencies | Acceptance | Model allocation |
| --- | --- | --- | --- |
| ED-20 — runtime/app identity and installation design | Platform + application catalog + Shell; audit installed packages and current QindaLutris first | Reproduce two independent Android app windows and two Windows programs; exact identity map, lifecycle/security/package contract and feasible runtime choice recorded in a proposed then reviewed ADR. No implementation during this audit. | Sol 6.1 research/design draft; Opus for the hard process/authority decision and independent critical review. |
| ED-21 — Android application lifecycle | Platform runtime adapter; ED-20 | Graphical setup/registration/launch/stop/remove; launch from ordinary Applications; runtime cold/warm/restart cases; isolate failure to the correct app; preserve app data. No Android-home-screen step required for normal app launch. | Sol 6.1 default; Sonnet for bounded UI. Opus only for difficult compositor/runtime faults. |
| ED-22 — Windows application lifecycle | QindaLutris/public runner and Platform; ED-20 | Register a non-game app, pin its Wine/Proton choice and prefix, launch from Applications, run two apps independently, handle child windows and remove a registration without losing another app or silently deleting data. Retain existing game-runner checks. | Sol 6.1 default; independent Sol review, Opus for prefix/data-loss or privilege design. |
| ED-23 — integrated green/blue application presentation | Catalog + launcher + Applications + tasks/container chrome; ED-20 and test adapters, then ED-21/22 | One stable identity across search/pin/running state; green Android and blue Windows badges; keyboard/AT names; no duplicates or generic runtime grouping; mixed native/Android/Windows container resize/detach at representative scales. | Sol 6.1 shared types/identity; Sonnet/Luna tightly specified visual bindings and fixtures; Sol review. |
| ED-24 — files/input/permissions and compatibility qualification | Adapter owners plus portals/terminal/agent consumers; ED-21–23 | One complete real-app journey per platform: launch, resize, type/IME, clipboard, open/save file, audio, notification, minimize/restore, close/relaunch. Then multi-monitor, runtime failure, lock/suspend and permission revocation. Record unsupported features per tested app. | Sol 6.1/Sonnet integration; Luna matrix/evidence; Opus only new trust or difficult lifecycle defects. |

Run a small private fixture matrix before physical trials; measure idle CPU,
memory, battery impact, startup and cleanup on qinda and the laptop. Runtime
processes should have an explicit background/idle policy, not remain expensive
forever after one app closes. Offscreen tests do not establish GPU/video or
game-controller compatibility.

Publish tested application versions and limitations. Assess ARM-only Android
packages, Google services/DRM, hardware codecs, Windows driver dependencies and
anti-cheat separately; do not equate a polished launcher with compatibility.
When an app cannot run, the graphical flow must explain the known reason and
offer a useful next step. No terminal workaround is required for a workflow
advertised as supported.
