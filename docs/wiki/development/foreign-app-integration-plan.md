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


## Private Windows initial server lifetime proof

The retained v9 attempt observed two fixed built-in windows and independent
server geometry/close steps, but rejected its second prefix's ambiguous
wineserver -k exit1. Its stop diagnostics were discarded, so an already-exited
server remains a source hypothesis rather than an observed cause. Window
absence is insufficient cleanup proof.

The next source-only fixture owns a foreground persistent server per fresh
prefix. It pins no-follow prefix/server-directory/regular-lock/socket identity,
checked Linux x86_64 flock ABI, the initial unreaped Popen/pidfd lifetime and
actual listening socket peer. Lock acquisition alone is not readiness. Loss or
replacement of the initial server permanently rejects the journey; a later
auto-start cannot restore it. No WINESERVER override suppresses the installed
loader's sibling-server precedence.

Normal retirement requires SIGINT through the owned pidfd, actual exit0,
wait/reap and held/current lock release without replacement. Forced containment
is separately recorded failure. Checked driver subreaper accounting retains
observed descendant lifetimes, reaps adopted children and refuses unknown
survivors without signaling them. Unknown-process environment or numeric
ancestry never grants signal authority. Prefix/server logs and data remain;
no -k/-w, bare PID/group signal, prefix deletion or host access fallback.

Pure and actual synthetic Python/kernel child/file tests qualify these
collaborators only. Installed Wine, private compositor/two-window operation and
complete per-prefix retirement still require different-author source review,
a fresh granted preflight and separately granted one-attempt native run.
This is a trusted-built-in experiment, not production Windows integration,
authenticated blue origin, rendered resize/input or ordinary app data support.

The owned-server fixture rejects expiry before and after blocking readiness,
retirement, and release checks. Expiry is latched: failure containment may still
settle held children, but cannot become successful qualification. Final driver
admission and receipt publication also check the overall deadline. Synthetic
child controls give every generation its own finite lifetime even without a
release file; the supervisor and outer test settle held owners and reap adopted
children on failure, retaining temporary data whenever settlement is uncertain.
These repaired controls require separate source review and execution; the prior
70-control receipt does not qualify the repaired source.


### Causal per-prefix supervisor source repair

The exact source635 native attempt refused an unknown adopted child before any
window steps. It observed and retired its two initial servers, but did not
qualify either app or the global child ledger. Polling ancestry could miss the
intermediate in Wine's real double-fork paths; the retained evidence does not
identify the triggering child. Faster polling or UID/prefix matching would
not establish the missing authority.

The next fixture source assigns each fixed built-in to a separate supervisor.
Each supervisor is a checked subreaper and kernel-childless before launching
only its own held initial server and app. A private inherited bounded
sequenced-packet control FD binds a fresh nonce, fixed domain and held
supervisor PID/start lifetime to every request/receipt. The driver independently
checks XRes, normal-window type and the fixed PE argument against the current
domain receipt. This is causal trusted-fixture membership, not production
authenticated app identity.

No observed or adopted process receives signal authority. Normal retirement
first settles the exact direct owners, then waits/reaps adopted children until
the kernel reports ECHILD; a nonblocking wait returning zero is not empty.
Server socket/peer/lock incarnation, initiating owner and deadlines remain
fenced before/after blocking work. Child census retains bounded PID/parent/
start/state observations including zombies only as diagnostics. Failure
containment signals only unreaped held direct owners, and outer namespace
retirement is never promoted into successful per-app cleanup. All source and
synthetic controls require exact safety review before any new generation,
preflight or Wine execution; source authoring adds no Windows completion.

The next private trusted Windows fixture source adds real XTest key events to
each admitted fixed Notepad/WordPad client. Current XRes PID/starttime and causal
domain membership are checked before/after and throughout input; focus must
remain within that client. The two distinct fixed ASCII sentinels must round-trip
through Ctrl+A/C and the same private X display's UTF8 clipboard. Small client
pixel captures before/after must differ. These prove separate input/readback and
pixel-change witnesses, not physical keyboard input, OCR of rendered text, or
trusted blue identification. Both typing receipts are mandatory before the
unchanged resize/independent-close and held-server/domain cleanup admission.
No host clipboard, display, provider data or user files enter the fixture.
The added libXtst is an existing Portage-owned fixture dependency, not installed
outside Portage. Source/pure controls do not qualify actual Windows usability.
