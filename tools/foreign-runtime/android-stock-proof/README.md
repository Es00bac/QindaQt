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
