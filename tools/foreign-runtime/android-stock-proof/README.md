# Stock Android private KVM proof — source candidate

This test-only fixture uses unchanged stock Waydroid1.6.3 and the accepted
September27 Vanilla Android images. It is not an Android fork or a production
backend. No VM, guest, mount, Binder operation, initializer, package or service
has run during source authoring.

The manager selected a minimal KVM fixture on October8. Qinda's installed
Portage kernel6.18.48 has Binder IPC/binderfs built in, virtio block support and
matching modules. QEMU, KVM access, cpio and gzip exist. Stock Waydroid/LXC
are not installed there. Platform owns the separate package closure and fixed
image qualification, with the current resolver refusing LXC's missing
libcap[static-libs] dependency. A data-package success does not close this.

## Containment and immutable inputs

run_vm.py launches only fixed QEMU argv: KVM, eight virtual CPUs,8GiB guest
RAM, no NIC, display, monitor, audio/GPU, shared filesystem, host home, buses,
devices or project trees. Only held readonly kernel/initramfs/system/vendor
regular-file descriptors are inherited. KVM is QEMU's required host device.

The owning manager scope must provide <=8 CPU quota, affinity within
0-5,12-17, <=12GiB memory, zero swap, tasks<=256 and nice>=10.
The launcher verifies the actual current cgroup files/affinity/priority and
fails closed if these constraints are absent. No automatic scope fallback.

The input manifest is schema1 with exactly inputs.kernel, initramfs, system,
vendor; each has an absolute path, exact byte size and sha256. Inputs must be
root-owned non-writable regular files, admitted through held nofollow FDs.
The two image hashes are fixed in source. Kernel and generated initramfs hashes
must be separately qualified and frozen by the manager; their mere inclusion
in a manifest does not approve them. Input inode/version readbacks are retained.
Trusted root can still modify files; no adversarial-root immutability claim.

A disposable fixture initramfs contains a selected, signature-qualified
Portage userspace closure and generated fixture config. boot_plan.py emits the cpio/gzip argv and bounded archive path list only. The stage must include the exact runtime closure, kernel
modules, Python stdlib/site-packages/GI typelibs, Qt plugins/data/font/NSS
dependencies . No package downloading, dependency solving or
host /etc copying is delegated to the boot builder.

prepare_overlay.py generates only a fresh retained fixture root under the manager private0700 run directory, with no package or runtime action. The fixture overlay supplies /init from guest-init.sh and /proof containing
guest.py, windows.py and scenario.json. It supplies empty proc/sys/dev/run/tmp/
var directories, merged-/usr symlinks, and generated /etc/passwd + group for
root0/proof1000, hostslocalhost, nsswitch files/dns, and selected public font
configuration. No shadow database, credentials, host machine-id or host
resolver configuration is copied. Generate a fresh machine-id inside the
guest before its private buses start. The final initramfs contents, executable
modes, generated files and package correspondence require independent review.

System and vendor images are read-only virtio disks /dev/vda and /dev/vdb.
Guest-only symlinks select them at stock's preinstalled-image paths.
The inspected initializer explicitly supports regular OR block-device inputs.
Stock mount -o ro therefore operates on guest block devices; no host loop
mount or filesystem export is necessary.

## Guest sequence and bounded evidence

guest-init.sh is fixture PID1, mounts only its own proc/sys/dev/tmpfs/cgroup
and prepares the two disk aliases. guest.py initializes a fresh runtime with
no /etc image override, requires exact offline config with both OTA fields
None, updater-disabled and SwiftShader properties, then starts private buses
and stock container plus nested QindaQt.

windows.py starts the stock user session, enables and reads back multi-window,
then launches Calculator and Clock in sequence. It observes one new distinct
ordinary-window record after each launch while preserving the previous set.
The fixed public Compositor1 owner is pinned. These sequential observations
are not authenticated per-app ownership; captions/applicationId remain reported
metadata. No force-stop, app grants or green origin authority is derived.

The first fixture currently records the two-window observation. Rendered
input and independent resize/close remain explicitly false and require the
next bounded extension after actual first-window readiness. Thus even a
successful first run is not ED20/ED21 completion or a full app journey.

Guest cleanup requests stock stop and independently reads LXC STOPPED.
Held direct children are settled; the serial result records inner cleanup
separately from host QEMU retirement. Host execution is bounded to300s plus
owned process cleanup,32MiB output and a fresh retained mode700 evidence path.
Only the held unreaped QEMU/pidfd may receive termination signals, never a
discovered process, host group or ordinary service. No descendant-empty claim
is inferred from QEMU termination. Failed guest/cleanup/readback stays failed.

## Exact next gates

1. Source review of these collaborators and concrete boot-input closure,
   including missing module/library checks and generated configuration.
2. Platform package qualification; root prepares protected immutable inputs.
   No guest is started by package installation.
3. Separately granted fixture initramfs creation; inspect every archived path,
   object type/mode, package/source hash, kernel and image binding. Freeze
   actual generated-initramfs digest and input manifest before any boot.
4. Different-author admission of exact QEMU argv/input/cgroup/evidence packet.
5. Root may grant one private boot/two-window attempt. Preserve its first
   refusal before a source repair or retry.

Pure source tests and syntax checks are separate from all five gates.

The boot plan uses cpio/gzip rather than dracut sysroot includes: installed dracut prepends its sysroot to external include paths. The frozen unfinished183b draft preserved that unqualified approach; no boot asset was generated from it. Cpio receives only a separately frozen prepared root, NUL-delimited paths and no symlink traversal. Stage package closure/signatures and every archived byte remain independent admission gates; shape checks alone do not authenticate them.

The concrete [runtime input plan](RUNTIME-INPUTS.md) and runtime-inputs.json freeze the13-package resolver result, eight proposed build prerequisites, hook constraints and the mandatory full stage closure. The boot builder now requires a separately admitted complete object inventory and enforces REQUIRED entries; no runtime asset is inferred from a present directory.

## Fixed laptop profile (source candidate)

The protected four-input manifest may explicitly set `resourceProfile` to
`laptop`:4 vCPUs,4096MiB guest, native `-cpu host`. The same fixed profile
governs live cgroup admission: at most5GiB memory, zero swap, four CPU quota,
affinity contained in0-7,256 tasks and nice at least10. Source/package work
uses jobs4/load4. The default/missing profile remains the original qinda8CPU/
8192MiB guest and12GiB host policy; unknown profile names refuse before launch.
All four held readonly inputs and the no-network/no-GPU/no-audio/no-host-export
QEMU options, deadlines and owned-process cleanup remain unchanged.

Laptop0-7 were observed to be four physical cores with both SMT threads,
leaving four other physical cores outside this fixture. Installed znver3 and
x86-64-v3 payloads stay on that native Zen3 host; this is architecture evidence,
not a successful boot or general CPU compatibility certificate.

The guest-only overlay declares nobody65534 for the stock network helper,
fixed python3→python3.14, sh→bash, awk→gawk and usr/sbin→bin aliases. It never
copies host accounts or alternatives configuration. `staged_mode` permits
only the exact installed mount/umount4755→0755 normalization; every other
privileged mode still refuses. Source bytes and source/stage modes must be
bound in the input manifest. Archive admission remains strict.

The laptop input receipt binds an installed public-file inventory separately;
it does not authorize a guest or assert complete dynamic loading. After
independent source/input review, the next gate is a fresh protected stage,
all REQUIRED entries/aliases and dependency checks, archive digest admission,
then a separately granted one-shot VM. No host package merge is needed if
that stage qualifies. The existing qinda prerequisite proposal remains a
fallback, not an action executed by selecting this profile.

## Protected installed-input copier (source candidate)

copy_stage.py is the single missing stage copier; it is not a boot or archive
runner. The manager must first grant and prepare a fresh root-owned0700
/var/tmp/qindaqt-android-laptop-stage-* root, protected inputs/plan.json,
inputs/inventory.json and exact source helpers under inputs/source.
The command is /usr/bin/python3 -B INPUTS/source/copy_stage.py RUN_ROOT
PLAN_SHA256 under the separately observed laptop4CPU/5GiB/nice10 envelope,
with a cleared environment and fixed public PATH. No imports from staged
payloads occur. The protected plan pins the copier itself and every owning
source; the inventory binds all installed VDB CONTENTS/public bytes. The
manager validates those protected inputs before launching Python, since
Python imports the owning helpers before the copier's own hash check.

The copier validates the current172 VDB inputs, creates the reviewed overlay,
then copies only canonical public /usr entries through nofollow parent/file
descriptors. Source size/version/SHA256/VDB MD5/mode are checked. Regular
outputs use exclusive creation and are new single-link root-owned files;
source hardlinks do not confer destination linking authority. No xattr,
capability, ACL or source ownership is copied. Symlinks retain exact admitted
targets and must resolve within the guest root. Only reviewed mount/umount
privilege normalization is allowed. Missing real parents are generated0755.
All partial outputs are retained on refusal; there is no cleanup authority.

The exact complete expected inventory is built from admitted package hashes
and the generated overlay before public copies, not blindly learned from
whatever bytes appeared in the final stage. Existing stage admission checks
all paths/modes/hashes and REQUIRED targets; VDB, protected source and inventory
pins are rechecked. Success writes the stage inventory and pure cpio plan,
not an archive. The operation bounds20,000 public objects,2GiB copied bytes,
256MiB per-file memory and120s cooperative time, with the manager owning the
external deadline. A success receipt is admissible only with actual process0,
bounded final settlement and current resource evidence; a late final-write
deadline refusal is not overridden by an earlier receipt field.

Dummy file controls exercise copied-inode independence, input mutation/modes,
source/destination links, FIFO refusal, existing output preservation, deadlines,
VDB pins, guest-rooted links and exact privilege exceptions. They establish no
real laptop stage, complete dynamic runtime closure or boot compatibility.


## First guest diagnostic boundary

The first actual laptop VM boot reached stock Waydroid init, which failed before
container startup or app windows. Its serial log and unqualified guest cleanup
are preserved. Stock init and stop now use --details-to-stdout so the next
separately admitted disposable guest exposes the actual stock diagnostic on
the already bounded serial channel. This changes no success, isolation,
resource or identity admission. No second boot is implied by this source change.

The guest now creates /var/lib within its fresh /var tmpfs before stock calls.
The pinned stock launcher uses nonrecursive mkdir for /var/lib/waydroid before
logging initialization, making the absent parent a concrete source explanation
for the first failure. This remains an inference until the repaired boot;
stock diagnostics remain enabled and no host directory is adopted.

The second actual guest reached binderfs/configuration, then lxc-info could not
resolve libgcc_s.so.1. The object was already in the admitted GCC15 package
closure; normal Gentoo resolves its nondefault directory through the host
loader cache. The fixture now sets only the fixed guest
/usr/lib/gcc/x86_64-pc-linux-gnu/15 library path after clearing its environment.
No host loader cache/config or package bytes are copied or modified. Static
DT_NEEDED search checks qualify directly reached native executables separately;
they do not prove plugin loading or a successful Android/container journey.

The third guest completed stock initialization and independently observed
LXC STOPPED during cleanup, then failed with PermissionError before windows.
The protected host stage's root and generated usr/bin are0700. Guest PID1
normalizes only its disposable / and /usr/bin copies to0755 and uses022 while
stock creates shared runtime/configuration; explicit private HOME and runtime
remain0700. The private guest bus parent is0755. Host stage protections remain
unchanged. Fixed operation-stage, errno and bounded256-character exception
diagnostics distinguish later failures. No app success is claimed.


### Private stock Pulse prerequisite (source candidate, not Audio1)

The sixth guest reached LXC mount setup but refused the stock mandatory
`/run/user/1000/pulse/native` bind. The guest now starts the installed
PipeWire and pipewire-pulse foreground binaries as UID1000 before the
Waydroid session. Their private native sockets must report the exact held
child PID/UID/GID, unchanged process start and installed executable inode.
Two pidfds remain held through bounded child cleanup. No WirePlumber, host
socket, host sound device or host daemon is used. The additional77
Portage-owned public module/config/binary inputs total5,948,275 bytes;
all original6730 input records remain byte-identical. Actual new stage,
archive and guest qualification are pending.

This endpoint is only a stock-container prerequisite. It does not qualify
Android audio integration: real app streams must later enter QindaQt's
Audio1 graph, retain trustworthy app association, support per-app gain/mute
and configured bus/output routing alongside native playback. The guest
receipt explicitly keeps audioIntegrationQualified false.

The boot-readiness property query uses only the remaining existing 190-second window-proof budget; other stock commands retain ten seconds. A timed-out command is never retried. Bounded stage/stdout/stderr diagnostics are retained in windows.json and guest serial, including whether the compositor baseline and boot-completed value were actually observed. This does not qualify app journeys or Audio1 integration.
