# Stock Android KVM source candidate — no execution admission

- Exact source: f67653a03ddb98f958d536f5a4d777d512c36fee.
- Base: manager37f1be1a9297ccf7f79d6c08daa586227c90710b.
- Worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-android-stock-runtime-20261008.
- Branch: worker/everyday-android-stock-runtime-20261008; explicitly pushed to /home/cabewse/git/container-wm.git.
- Ownership: tools/foreign-runtime/android-stock-proof/ only plus own board/replies. No shared wiki/registry edit. Earlier unfinished183b preserved, including the superseded dracut include plan.

## Concrete proposed route

Root selected a minimal disposable KVM fixture, using existing Portage QEMU/kernel and selected qualified userspace. Kernel Binder/binderfs is built in. Guest-only tmpfs owns /etc,/run,/var and all stock Binder/LXC/network/mount/clock effects. There is no host home, bus, GPU/audio, device passthrough, shared filesystem or network adapter; QEMU needs only host KVM.

Unchanged installed stock1.6.3 initializer.py55–57 explicitly accepts regular files OR block devices in preinstalled images. Two held readonly system/vendor image FDs become guest virtio disks, with generated guest-only aliases. Stock mount operates inside the guest, so no host loop mount is required. Images/Calculator/Clock remain unchanged.

The source contains fixed QEMU argv, actual current resource admission, held image/kernel/initramfs descriptors, bounded QEMU observation/cleanup, generated guest root config, pure cpio/gzip boot plan, guest PID1/stock phases and nested-compositor two-window observer. No custom runtime/backend/image fork or claimed per-app permission producer.

## Actual source-only checks

- Nine pure controls pass0,0.005s: resource bounds, descriptor alias/extra input refusal, no network/host export argv, generated config/source shape, existing output refusal, unexpected root entry and symlink non-traversal.
- Eight Python AST parses, shell syntax0, diff0.
- Wiki links534, strict MkDocs0/8.19s. No shared wiki change; owning fixture README documents pending inputs and limitations.
- Raw .cache/android-stock-source/{pure-tests-final.log,static-final.json,docs.log,mkdocs.log,source-freeze.json}.
- Source-freeze SHA256: 6b8fd26c0e41826a5e826cd137cac314de8a5d34774970e5cc7bf032052bf0f8.

These are not VM, guest, ordinary-window or process-cleanup test results. No real child/VM, initramfs build, mount, Binder, namespace, service, package or installed action occurred.

## Required review and next executable gates

1. Different-author exact SOURCE review of the fixed launch boundary and guest phases, particularly input ownership, current cgroup admission, held process cancellation/cleanup and separate inner stop readback. Source tests do not qualify these actual runtime paths.
2. Platform/root must supply a signature-qualified selected userspace/kernel-module closure. Current runtime resolver requires private libcap[static-libs] for LXC; Waydroid is absent on qinda. Missing closure is explicit, not inferred from package assignment.
3. Prepare only a fresh fixture root from the selected package images and generated files. Freeze every path/hash/type/mode and required non-ELF Python/GI/Qt/font/NSS data. boot_plan.py emits cpio/gzip vectors over that exact root; it does not solve dependencies, authenticate package inputs or build anything.
4. Separately grant bounded boot-asset creation, inspect complete initramfs contents and bind its actual digest plus kernel/tool/image/package identities. Actual tools and inputs must be reviewed; a caller-supplied manifest is not authorization.
5. Only then grant ONE exact VM attempt under qinda8CPU quota/affinity0-5,12-17/12GiB/swap0/tasks256/nice10. Host result requires both manager-observed settled process exit0 and the matching final result; result.json alone cannot override cancellation/deadline/final-write failure. Guest STOPPED and QEMU retirement remain different facts. Preserve the first refusal before repair/retry.

The first runtime candidate observes sequentially launched Calculator and Clock window records. Rendered input and resize/close are explicitly false/open, and sequential window metadata cannot authenticate app identity. There is no ED20–24 completion, platform badge authority or actual Android window claim.

## Image packaging first failure and repair

Actual original29cbc Portage unpack ELOOP is preserved by Platform526c3. Exact owning overlay repair486feb2b32851b682eb7f4e2703b32f2b798aaf9 uses PORTAGE_ACTUAL_DISTDIR with unchanged nofollow/hash/type/version verifier;15 tiny tests pass. Desktop handoff f9bee62e records the exact evidence. Same Platform source review precedes a new separately granted package build. No generic retry or host install was performed.
