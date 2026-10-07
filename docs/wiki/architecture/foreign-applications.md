# Foreign application identity and lifecycle

This is the **proposed ED-20 contract**, owned by
[ADR-0352](../adr/0352-foreign-runtime-identity-and-installation.md).
It is not an implemented Android/Windows desktop service. The
[delivery plan](../development/foreign-app-integration-plan.md) owns the visible
green Android / blue Windows requirements and ED-21–24 acceptance.

## Existing boundaries and proposed ownership

ApplicationCatalog already scans bounded XDG entries for launcher and File
Manager Applications. WorkspacesApps resolves exact IDs and unique aliases.
The compositor's foreignwindowidentity helper repairs opaque display labels;
its argv and class inputs are untrusted hints. None is an origin authority.

QindaLutris lives in this repository, under src/apps/qindalutris, rather than
a separate QindaLutris hub on the audited host. Its launch planner exposes
game-shaped private types; umu_launch already pins exact builds and strips
override variables. ScopedGameLauncher creates scopes per launch. Reusing
that scope as per-app ownership is unsafe when multiple launches share a
Wine prefix or launch services outside the tracked scope.

| Owner | Proposed public responsibility | Excluded responsibility |
| --- | --- | --- |
| ApplicationCatalog | Stable registered entries; origin/registration revision; merge and launch route values | Guest install, process authority, shell rendering |
| Foreign runtime protocol | Bounded copied values, exact schema codecs, state/intent validation | Bus, filesystem, UI and OS evidence collection |
| Foreign runtime broker/client | Current-session producer admission; owner/generation fencing; async typed operations | Portage privilege, global window control |
| Android adapter | Package/task inventory, lifecycle and verified guest task-to-surface mapping | Container topology and invented identity from titles |
| Public Windows runner extracted from QindaLutris | Exact Wine/Proton recipe plan, sanitized argv/environment, owned lifecycle evidence | Game library, stores, shell private imports |
| Guest recipe/materialization module | Portage-installed recipe validation; private transaction/receipt/data retention | Download manager, root home writes, arbitrary setup files |
| Compositor and shell | Window incarnation/evidence; normal grouping, focus, resize and member-origin rendering | Guessing process authority or killing shared runtimes |
| QindaPortage | Preview/authorize/apply package plans via existing privileged boundary | Opening a user's prefix as root or inferring guest installation success |

These are proposed boundaries, not existing exported CMake targets. Changes to
the [module registry](module-boundaries.md) occur with the implementing slice.

## Typed values and bounds

Version 1 uses strict, length-bounded records with fixed enums; unknown fields,
enum values, duplicates, invalid UTF-8/control characters and excess bounds
reject the complete snapshot. A public Qt Core values target and independent
codec precede adapters. Copies own all strings/arrays; borrowed ports outlive
their single event-thread coordinator. Blocking runtime/filesystem work runs
off that thread and returns through an owner/generation fence. No transport
object or pointer crosses a serialized boundary.

| Value | Required fields / limits |
| --- | --- |
| AppKey | Origin enum Android/Windows; adapter-generated UUID registration; immutable recipe identity; never a title, filesystem path or desktop alias |
| RecipeIdentity | Portage CPV/repository/slot, SHA-256 of exact installed recipe and payload, exact runtime pin; validated against trusted root-owned package state |
| Registration | AppKey, revision u64 (positive, no wrap), recipe identity, private runtime profile key, launch target, supported capabilities; label 256 UTF-8 bytes, icon reference 256, arguments at most 64 × 4096 bytes |
| RuntimeBinding | Session incarnation, admitted adapter-instance incarnation, runtime incarnation; all nonempty 128-bit values minted by their owners |
| Operation | Caller operation UUID, AppKey and expected registration revision, exact RuntimeBinding; enum action; at most 64 in flight globally and one mutation per app |
| WindowAssociation | Exact RuntimeBinding, AppKey/revision, launch UUID, compositor window ID plus incarnation, verified runtime task/process evidence; optional parent association must share live AppKey/binding |
| Snapshot | Protocol version 1, producer binding, monotonically increasing revision, at most 4096 registrations / 4096 associations; encoded total at most 4 MiB |
| Result | Operation/binding, phase and error enums, sanitized explanation at most 1024 bytes; no argv logs, guest paths, tokens or document payload |

UUIDs alone are not credentials. The concrete wire must bound lengths before
allocation, never parse a larger message then truncate it. Runtime/profile
keys are opaque handles; the broker resolves them to private state. Launch
target is a recipe-declared Android package/component or Windows relative
entrypoint, not an arbitrary program/command. Recipe-local paths reject
absolute paths, traversal, NUL and link escape; handles and identity checks
must survive a concurrent filesystem replacement, not just a string prefix
test. Guest package name/signing identity is verified before registration.

Persistent schema v1 uses bounded atomic replace, safe ownership/mode and
symlink-resistant access. Unknown/new schema is retained intact and exposed as
unavailable for repair. Runtime/window/operation incarnations are volatile and
never restored as authority after login or crash. Revisions fence same-ID
remove/re-register and recipe updates. All affected public static-library
consumers rebuild together; wire peers with another version fail unavailable.

## Admission and window association

The supervisor binds a broker to the current authenticated session and
provisions private adapter channels for its exact children. The receiver
checks kernel peer credentials against the supervised process lifetime
(pidfd/start identity), expected installed executable and descriptor lineage.
Compositor attachment uses the existing public session/compositor authority;
it does not accept arbitrary callers that claim the broker's well-known name.
Normal applications never receive adapter descriptors.

The proposed evidence collector must bind facts to the exact live compositor
client/window incarnation. For native Wayland Wine clients, kernel connection
credentials can anchor a qualified process; XWayland's property PID cannot.
An XWayland path needs compositor/X server resource-client evidence qualified
for that server incarnation, otherwise it stays unknown. A PID plus start
time in a public message is still only an assertion until the receiver's
trusted collector obtains and validates it.

For Waydroid, a renderer PID can identify a runtime but cannot distinguish
guest apps. Android association requires a runtime-owned task/package mapping
to the exact Wayland surface exported by that runtime, validated inside the
admitted bridge. It must prove guest package/UID ownership, including activity
handoff and child dialogs; an app-controlled app_id/package-shaped class is
insufficient. This mapping is an explicit feasibility gate. If stock Waydroid
cannot provide it, a narrowly packaged bridge extension needs its own reviewed
implementation; do not grant trust to hints to avoid that work.

A read-only source trace sharpens this gate: at Waydroid vendor tree
\`1b95b85221f4faaa357932fa5e93eacb7430f636\`, the
[WindowStateAnimator patch](https://github.com/waydroid/android_vendor_waydroid/blob/1b95b85221f4faaa357932fa5e93eacb7430f636/waydroid-patches/base-patches-33/frameworks/base/0007-wm-Include-task-id-in-surface-name-for-easier-tracki.patch)
prepends the task ID to the window's title. Hardware tree
\`6e898e9d18f442873305f4992df6e6148aa1e693\` then
[parses the apparent app ID from that layer-name text](https://github.com/waydroid/android_hardware_waydroid/blob/6e898e9d18f442873305f4992df6e6148aa1e693/hwcomposer/modes/waydroid_mode.cpp)
and [publishes it as the Wayland app ID](https://github.com/waydroid/android_hardware_waydroid/blob/6e898e9d18f442873305f4992df6e6148aa1e693/hwcomposer/wayland-hwc.cpp).
Thus even an authenticated HWC connection does not authenticate the package
claimed by that string. For example, title-shaped text naming a peer package
survives this parser independently of the actual guest UID. This is a static
data-flow counterexample, not an observed runtime exploit or a claim that
those source trees exactly match the uninspected image payload. A viable
bridge must obtain task/component ownership from guest system authority,
validate the owning UID/signature and bind it to the exact exported surface;
otherwise association remains unknown.

The threat boundary protects against malformed apps, spoofed metadata,
unadmitted producers and stale/replaced services. It does not claim sandboxing
against root or a fully compromised host user capable of ptracing the broker.
Android runtime compromise invalidates all mappings from that runtime.
Origin metadata grants no content access, capture, input, secrets or agent
authority.

## Lifecycle and failure semantics

States distinguish MissingRuntime, MissingImage, NotMaterialized, Ready,
Starting, Running, Stopping, Unavailable and NeedsRepair. Runtime state, app
process state and observed window count are separate facts. Successful spawn
is Starting, not Running; a hidden background app is not declared stopped just
because its window count is zero. Startup waits at most 60 seconds, warm app
launch 30 seconds and graceful close 10 seconds before reporting a bounded
result. Materialization has recipe-bounded timeout up to 30 minutes with
progress and cancellation; limits do not prove cancellation completed.

Completion phases are Accepted, Completed, Failed, Cancelled and
OutcomeUnknown. Timeout after dispatch or lost authority yields OutcomeUnknown
unless the owner proves a terminal state. Do not automatically replay a
mutation. Repeated operation UUID returns its retained result for the same
binding, never repeats work; different arguments under that UUID are rejected.
Keep at most 256 terminal receipts per adapter incarnation, then return
UnknownOperation for evicted requests rather than replay.

On producer loss/restart, remove live associations, cancel local waits and
invalidate all old operation/launch tokens before admitting a fresh snapshot.
Runtime restart does the same even if the adapter process remains. An old
compositor window ID reused in a new incarnation cannot inherit a badge.
Newly observed legitimate surfaces require fresh association evidence.
Disconnect/reconnect cannot reanimate an old parent relationship.

CloseWindow uses ordinary compositor authority for exactly one window.
StopApp is separately advertised and validated: Android package/user scoped
stop requires runtime proof of independent ownership; Windows force stop
requires exclusive per-app prefix scope ownership and no peer apps. Scope
emptiness/process termination must be observed before Stopped. A scope name,
successful stop command, or launcher exit is insufficient. A shared runtime
stays alive while any peer app or operation needs it; idle suspend uses adapter
inventory plus an explicit idle policy, never the disappearance of one window.

## Portage package and private materialization contract

An overlay guest recipe packages a licensed, fixed-digest payload and schema-v1
manifest under a root-owned data directory. It declares platform, architecture,
package/signing identity, runtime pin, entrypoint, exact installer mode,
bounded installed-file manifest or verified guest inventory, supported update
policy, timeout and data-retention rules. Unspecified installer behaviors are
unsupported. System services/defaults and Android images are separate Portage
packages. Prefer package-supplied Wine builtin fixtures for initial proof.

The graphical flow delegates missing package installation to QindaPortage,
then asks the unprivileged adapter to materialize the exact installed recipe.
No raw URL, arbitrary APK/MSI/EXE or arbitrary argv is accepted at that boundary.
The materializer validates package provenance and payload digest immediately
before use through stable file handles. It stages into a private per-app
profile, verifies resulting app identity/inventory, then atomically records a
receipt (recipe digest, app revision, runtime pin, result inventory).
A crash leaves pending/repair state, never a fabricated successful install.
Only one materialization/update operates on an app; live app update is refused.

Executable copies required by Windows/Android in mutable private storage are
covered by the package recipe and materialization receipt. They are not
unmanaged application downloads. Updates invalidate the old receipt until
reverified. Unexpected self-updated executable contents cause NeedsRepair and
block managed relaunch; recipes requiring unavoidable self-update are outside
the supported set. APK identity checks include signing certificate and ABI;
Android's package manager commit must be inspected, not inferred from CLI exit.

Registration removal retains private data by default. Package removal makes
dependent entries unavailable, preserving their data and pins. Explicit data
deletion is a separate previewed operation, verifies exact profile ownership
and no live app/peer users, and never traverses external links. Shared-prefix
deletion is refused. Ebuild uninstall does not remove per-user data. Recovery
can retry or discard an incomplete materialization without touching peers.

Current QindaLutris downloads/store installs and user-directory Proton discovery
do not satisfy this contract. They are not reused for ED-21/22. The game owner
must separately redirect or disable affected direct-software-install actions
to packaged recipes before a whole-desktop Portage-only claim. Existing games,
untracked prefixes and configuration are preserved and labelled accurately.
An unmanaged executable does not become managed merely by registering its
current hash.

## Catalog, presentation and permissions

Managed IDs occupy an adapter-owned namespace and survive relaunch/upgrade;
one AppKey maps to one catalog entry even when many windows exist. Native XDG
override semantics stay intact. A shadowed managed desktop entry loses its
managed launch/provenance attachment unless the public catalog can demonstrate
the exact current managed registration. Pin migration uses explicit mappings,
not alias similarity. Duplicates are diagnosed, never silently assigned.

Authenticated associations drive per-window Android green / Windows blue
tokens and accessible origin descriptions. Unknown association remains
unclassified even if an untrusted hint matches a managed entry. Native window
labels/icons may still use existing display-only matching. Container chrome
keeps origin per member. Origin never replaces focus/error/selection semantics.

File grants, clipboard/input, notifications, audio and agent capabilities are
independent ED-24 integrations. No ambient home mount, Wine Z: exposure or
Android shared directory is assumed safe; supported recipes specify exposure
and the GUI communicates it. No semantic provider is inferred from a window.

## October 7 feasibility receipt and next executable gates

Read-only checks used desktop base
2188d8e0e339ce4b56acb841a4b570f58a3002cc and overlay base
aefd6c8bbd30f57731e1833fa72429793441e554. Shared checkout changes were preserved.

| Host / check | Observed result | What it establishes |
| --- | --- | --- |
| qinda command/package inventory | No waydroid command or package record | Android runtime unavailable on this host; no launch attempted |
| qinda-top Waydroid 1.6.3 status | Reports not initialized; no /var/lib/waydroid, image directory or config | No initialized Android image/app inventory; installed CLI is insufficient |
| qinda Portage CONTENTS | wine-proton-11.0.2 owns x86_64-windows/notepad.exe and wordpad.exe; GE-Proton 11.6 owns copies too | Two non-game package-tracked payloads exist; no window/runtime success inferred |
| overlay tracked paths | GE-Proton recipe exists; no Waydroid image/guest-app recipe in recorded overlay tree | Portage image and app packaging is a prerequisite, not an authorized ad-hoc download |
| QindaLutris scope source | Per-launch user scope; shared prefixes possible | Cannot adopt per-launch scope as guaranteed independent per-app termination |

Waydroid's [documented multi-window property](https://docs.waydro.id/usage/waydroid-prop-options)
supports exploring ordinary windows; its
[app interface](https://docs.waydro.id/usage/install-and-run-android-applications)
exposes list/launch/install/remove. Those facts do not demonstrate this desktop's
resize or association contract. The
[1.6.3 app-manager source](https://github.com/waydroid/waydroid/blob/1.6.3/tools/actions/app_manager.py)
can wake a frozen container while listing apps, so discovery must not be
described as side-effect-free merely because it is called list. This audit
used status only on the uninitialized laptop and did not invoke app list.

Required next gates:

1. Portage-provision a fixed licensed Android image and two appropriate
   guest app recipes in a designated test environment. Record actual initialized
   runtime, app/signing/ABI inventory; no Google/store dependency by assumption.
2. Under a manager-issued private nested display/runtime lease, launch two
   Android apps with multi-window mode and two Wine fixture apps in distinct
   disposable prefixes. Capture exact surface/identity maps, resize, child
   dialog, independent close/relaunch and runtime restart. Never change the
   user's physical Waydroid session or Wine prefix.
3. Prove the producer/transport mapping and spoof/restart negatives before
   accepting authenticated green/blue window origin. Test shared-prefix force
   stop denial, PID reuse, guest activity handoff and disappearing child windows.
4. Review ADR and exact candidate independently; implement public types/codec
   and extracted runner in separate slices, retaining game regression gates.
5. ED-21–24 then require graphical registration/install/recovery, catalog joins,
   input/files/audio/notifications, mixed containers, scales and physical trials.

Run the pure executable design proof with
`python3 tests/design/foreign_application_contract.py`. It exercises admission
policy from already-authenticated fixture evidence and cannot authenticate a
real transport or demonstrate a running app. Full documentation checks are
`mkdocs build --strict` and `python3 tools/validate-docs`.

## Portage provisioning input packet

This packet is for the Platform owner after the current release lease, not a
request to run ordinary upstream initialization now.

- Runtime: existing `app-containers/waydroid-1.6.3::guru` (GPL-3+ ebuild).
  Its dependencies include LXC/seccomp, nftables, dnsmasq DHCP, D-Bus, polkit,
  Python gbinder, GTK Wayland and an audio server. The current ebuild's
  `pkg_config` invokes ordinary `waydroid init`; without packaged preinstalled
  images that downloads software outside Portage, so it is not the managed
  provisioning path.
- Image package candidate: a new overlay data package owning
  `/usr/share/waydroid-extra/images/system.img` and `vendor.img`.
  Waydroid 1.6.3 explicitly recognizes this preinstalled pair and sets image
  OTA to None. Validate both regular root-owned files and exact hashes before
  initialization. Keep per-user Android data outside package-owned files.
- Upstream metadata observed October 7:
  [VANILLA system](https://ota.waydro.id/system/lineage/waydroid_x86_64/VANILLA.json)
  names `lineage-20.0-20260927-VANILLA-waydroid_x86_64-system.zip`,
  847449341 bytes, published id
  `053552725bf4ae25e2d3f7e6f1947203294aa9d73dc17fe1f3abe646e5d3bd8a`;
  [MAINLINE vendor](https://ota.waydro.id/vendor/waydroid_x86_64/MAINLINE.json)
  names `lineage-20.0-20260927-MAINLINE-waydroid_x86_64-vendor.zip`,
  189818687 bytes, id
  `d911b8353f6c807b94790b1c41c67e863a9f3dcd1cf3ec0232351398235afd7a`.
  Both version 20.0 payloads are hosted in the corresponding upstream
  SourceForge image directories. Metadata was read; payloads were not
  downloaded or hash-verified. The packaging owner must freeze actual
  Manifest digests and inspect image licenses/NOTICE and redistribution
  terms; the runtime ebuild's GPL-3+ license is not the image's license.
- Prefer two launchable, freely licensed built-in image apps for the first
  two-window proof, subject to actual APK package/version/signature inventory
  after package extraction. Their payloads then belong to the image package;
  separate recipe records point to exact embedded APK digests. Do not invent
  app package IDs or call these verified before inspecting the image.
  A later external APK recipe requires fixed artifact/version, source/license,
  signer, ABI and manifest digest; no store installation is an acceptable
  substitute.
- Both audited kernels are `6.18.48-gentoo-dist-bin`; read-only config shows
  ANDROID_BINDER_IPC, ANDROID_BINDERFS, MEMFD_CREATE, NAMESPACES and USER_NS
  enabled. No binder/binderfs device path was present in the inspected views;
  this is not a failed kernel support claim. The lease must qualify binderfs
  mounting/nodes, LXC namespaces/seccomp, network/nftables/PSI prerequisites,
  render-node permission and actual GPU path. qinda had renderD128; no GPU
  functionality was tested.
- Private proof must isolate Android container name/work directory, binder
  nodes, D-Bus/session service names, network and nested Wayland socket as
  supported by the packaged runtime. If stock Waydroid cannot isolate those
  global resources, use an explicitly reserved test host/account and a
  manager-controlled runtime lease, not a second conflicting live container.
  Normal physical session state must not be changed to simulate isolation.

The package installer should present the required downloads/space and user
data retention before applying its normal authorized plan. It must not
activate a new global service, alter networking or initialize guest state as
an unannounced side effect of the documentation or metadata probe.
