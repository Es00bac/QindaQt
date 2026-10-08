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

## Reassessed next outcomes — October 8, 13:06 MDT

Implementation remains paused for the owner's R20 login test and planning
checkpoint. The [revised delivery order](everyday-desktop-plan.md#reassessed-delivery-order-october-8-1306-mdt)
governs the next dispatch after resumption.

Use the existing Wine/QindaLutris path for a complete two-app lifecycle and
real text input, then wire public registration and authenticated blue identity.
The repaired ownership source has 21 focused controls; the next native plan
454573a85522704181d4e160cd5df1c5d573f8be is unexecuted and its frozen driver does
not yet prove keyboard/text input.

For Android, observe two stock Waydroid apps before deeper runtime work.
The accepted KVM fixture is private containment, not a new product requirement.
An existing qualified Portage runtime closure may shorten acquisition, but its
availability must be established; otherwise use the reviewed narrow route.
Checkpoint 2c52f6df4 preserves private preparation and backups only. Effective
access is unqualified; no package phase or guest started. Neither host
initialization nor bypassing the accepted isolation boundary follows from
this reprioritization.

ED-20–24 still require lifecycle, installation/data preservation, shared catalog,
green/blue identity, grouping and the complete supported app journeys below.
A first app window or a prepared image package does not close that scope.

## Current stock-runtime boundary — 2026-10-08T18:39:21.869979+00:00

The stock-first architectureda559 is integrated. Repaired image recipe486 now
has a real signed909588480-byte data package02f97185; root independently
verified all41 proof payloads/42tar members, current complete package and all11
image entries, including exact stock system/vendor bytes. New postpackage
15 owning controls pass; the original src_test summary remains unobserved.
No init/OTA/mount/guest or public whole-image correspondence claim follows.

KVM sourcef676/d174 and enforced runtime-input9d/b39 are independently source
accepted. Exact eight ordinary library/account prerequisites8601/02ce await
root protected-input preparation and ordinary Portage transaction. Account
creation and libcap ABI32/PAM effects are explicit; no daemon atoms/starts.
Guest closure/boot/two app windows remain unrun. Windows985 causal-domain
source046 is accepted, and21 bounded controls pass after the retained initial
cwd launchfailure. The rapid unit has configured rather than live-observed
resource/outer-start evidence; full Wine preflight/windows/cleanup, rendered
input/resize/independent close and trusted identity are still open.

## October 8 implementation progress

Android sourcec05d9c16/23ab470 and source reviewd97ea819 establish bounded
input inspection and a Portage-owned public AOSP verifier. The private signed
package6f6837a0 has bounded independent artifact acceptancee79edabc: required
signature/image/embedded recipe and28 indexed raw files/29 archive members match.
Actual Java25/API33 packaged10 and Calculator/DeskClock6 checks pass, including
matching-new-hash signed-entry poison and unchanged restoration. The authentic
unsupported-only signature fixture also refuses as expected. The original
build phase stdout gap is retained; no original test count is inferred from
emerge0. This package is not installed. Resolved whole-image source/license
correspondence, runtime startup and authenticated window identity remain open.
No Android app-window success is claimed.

Windows source-accepted v9 9721b98 has a fresh source/payload-bound preflight and
an actual private attemptbb732a4. Notepad and WordPad have distinct observed
XRes PIDs/prefixes and normal windows, with ordered server geometry/close steps
and final empty compositor inventory. The second prefix stop returns1, so the
complete cleanup admission correctly refuses. Empty windows and final namespace
retirement cannot replace per-prefix cleanup proof. The next fixture owns each
foreground persistent server's initial lifetime/readiness and verifies exact
retirement and orphan accounting. Server geometry is not client-render/input
proof; no production origin authority or green/blue presentation is delivered.
Prior XKB and authority failures are preserved. Neither platform has completed
the full compatibility journey.

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
| Input and media | Keyboard shortcuts, mouse, touch, IME, clipboard and scale behave predictably. Actual Android and Windows playback joins the PipeWire graph observed by Audio1 with stable app identity, per-app level/mute and the user's chosen mixer buses and outputs alongside native streams. Integrate notifications and portal-style permissions where the runtime can support them, with explicit limits where it cannot. |
| Management | App details show origin, runtime/prefix, version and compatibility status. Provide graphical install/remove and runtime settings; keep app data on uninstall unless deletion is explicitly selected. |
| Terminal and agents | Stable launch/list/status identifiers and structured outcomes through a public adapter. Origin and current capabilities are available to agents; native-looking windows do not imply a semantic document provider exists. |

### Audio1 acceptance for foreign applications

ED-24 uses a real audio-producing application on each platform. A stock
Calculator/Clock or Notepad/WordPad launch cannot qualify audio by itself, and
the private Pulse socket used to start an isolated Android test guest is not a
desktop audio route. The Windows runner and Android runtime adapter must expose
each playback stream, with its authenticated application identity, in the same
PipeWire graph that Audio1 observes. A user can set that stream's level and
mute through Audio1 and route it through the configured mixer buses to one or
more chosen outputs without bypassing native applications.

Acceptance records both Audio1 state/readback and an actual signal witness at
the selected outputs. Exercise simultaneous native and foreign playback,
independent per-app controls, a changed default device, output disconnect and
reconnect, and a fullscreen app. Preserve a user's explicit routing rather
than silently returning a stream to the default sink. Microphone/capture and
communications features need separate permission and routing evidence; an
output-only proof does not qualify them. If Android needs a host audio bridge,
record its process, identity and lifetime contract in a reviewed ADR with the
implementation; an isolated guest-only server cannot count as the Audio1
integration.

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

### Selective lessons from Google and Valve (October 2026)

[Googlebook's Android developer guide](https://developer.android.com/develop/adaptive-apps/guides/googlebook/overview)
describes desktop Android apps in freely resizable windows with keyboard,
pointer, multi-instance and cross-window drag/drop behavior. Use those as
compatibility scenarios for QindaQt's stock Android app windows and honest
app-quality reporting. Googlebook uses an Android technology base and ChromeOS
desktop foundations; QindaQt's Gentoo host and stock Android runtime remain
separate implementation boundaries.

[Valve Proton](https://github.com/ValveSoftware/Proton) supplies Wine-based
Windows compatibility, while the
[Steam Runtime](https://github.com/ValveSoftware/steam-runtime) pairs recent
Proton versions with predictable, versioned container environments. Extract
QindaLutris's existing UMU/Proton launch planning into a public ordinary-app
runner: pin the chosen runtime and prefix per app, preserve environment and
lifetime authority, and test actual window, input, controller, fullscreen and
Audio1 behavior. Running an installed Wine executable in the private two-app
fixture does not by itself qualify the complete Proton/Steam Runtime stack.
Adopt any additional open components through Portage and retain their licenses.

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

The table preserves the original planning packets. Current candidates,
reviewers, worktrees and next gates are recorded in docs/TASK_LIST.md and the
delivery queues. Dispatch uses a fresh exact hub base as specified in the
[main plan](everyday-desktop-plan.md#proposed-work-packets); source acceptance
alone does not complete a runtime or app journey.

| Packet | Owner and dependencies | Acceptance | Model allocation |
| --- | --- | --- | --- |
| ED-20 — runtime/app identity and installation design | Platform + application catalog + Shell; audit installed packages and current QindaLutris first | Reproduce two independent Android app windows and two Windows programs; exact identity map, lifecycle/security/package contract and feasible runtime choice recorded in a proposed then reviewed ADR. No implementation during this audit. | Sol 6.1 research/design draft; Opus for the hard process/authority decision and independent critical review. |
| ED-21 — Android application lifecycle | Platform runtime adapter; ED-20 | Graphical setup/registration/launch/stop/remove; launch from ordinary Applications; runtime cold/warm/restart cases; isolate failure to the correct app; preserve app data. No Android-home-screen step required for normal app launch. | Sol 6.1 default; Sonnet for bounded UI. Opus only for difficult compositor/runtime faults. |
| ED-22 — Windows application lifecycle | QindaLutris/public runner and Platform; ED-20 | Register a non-game app, pin its Wine/Proton choice and prefix, launch from Applications, run two apps independently, handle child windows and remove a registration without losing another app or silently deleting data. Retain existing game-runner checks. | Sol 6.1 default; independent Sol review, Opus for prefix/data-loss or privilege design. |
| ED-23 — integrated green/blue application presentation | Catalog + launcher + Applications + tasks/container chrome; ED-20 and test adapters, then ED-21/22 | One stable identity across search/pin/running state; green Android and blue Windows badges; keyboard/AT names; no duplicates or generic runtime grouping; mixed native/Android/Windows container resize/detach at representative scales. | Sol 6.1 shared types/identity; Sonnet/Luna tightly specified visual bindings and fixtures; Sol review. |
| ED-24 — files/input/permissions and compatibility qualification | Adapter owners plus portals/terminal/agent consumers; ED-21–23 | One complete real-app journey per platform: launch, resize, type/IME, clipboard, open/save file, Audio1-visible app playback with independent gain/mute and configured bus/output signal, notification, minimize/restore, close/relaunch. Then simultaneous native playback, default/output reconnect, fullscreen, multi-monitor, runtime failure, lock/suspend and permission revocation. Record capture and unsupported features per tested app. | Sol 6.1/Sonnet integration; Luna matrix/evidence; Opus only new trust or difficult lifecycle defects. |

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
