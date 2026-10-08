# Exact Android prerequisite transaction — SOURCE ONLY, no grant

- Time: 2026-10-08T18:07:09Z
- Accepted source9d0f21359442a7d714e15df88d33cd4ef902dad3; independentb39b09f37c6ffd8920fcdf2046be4361731e719a.
- Frozen argv:20261008T180709Z-astra-android-eight-prerequisites-argv.json; SHA256 e2feff3ba887a36d19a886d573b2b4248f3804b432ff2f1893ce9182de55917a.
- Three prospective phases only: signed old-libcap preservation, exact pretend,
  ordinary Portage eight-atom --oneshot --buildpkg merge. No command executed.
- Sole-heavy root grant required; no subsequent runtime package build/stage/VM
  is authorized by this proposal.

## Exact effects and absence of automatic daemon work

The eight version/repository-pinned atoms are libnftnl1.3.2,libcap2.78,
libglibutil1.0.82,gbinder1.1.52 and dnsmasq/LXC group+user atoms0-r3/0-r2.
Previous actual pretend0 shows7new/1reinstall and no additional package.
The normal full13 resolver and all13 build-only refusal remain preserved in
.cache/android-runtime-closure. No new dependency bypass.

Current installed libcap USE is abi_x86_64,amd64,elibc_glibc,kernel_linux,pam.
The task adds static-libs only, but normal existing policy also adds ABI32:
new package builds both32/64 cap/psx libraries/static archives and PAM modules.
PAM stays enabled; libcap ebuild deletes upstream-installed security dir from
its image and explicitly installs pam_cap through pam.eclass, plus protected
capability.conf. This is an actual host library/PAM package replacement,
not an inert data-only operation. Existing shared-library processes may retain
old mapped bytes; no service reload/restart is requested or inferred.

Account eclasses create system group/user names dnsmasq275 and lxc358, with
primary same-name group, default /dev/null home and /sbin/nologin shell.
The source has a postinst usermod branch for existing users and may choose an
alternate numeric ID when requested ID is occupied. This plan refuses either
name/ID collision or a config override before merge rather than accepting such
behavior. Read-only local /etc/passwd and group checks show allfour name/ID
presence booleans false for each pair; current NSS resolution and effective
ACCT_* override checks remain root admission before execution. No password or
shadow content was read/logged by this source review.

Library recipes have no explicit service-start hook; account side effects are
through the owning eclasses. Waydroid,LXC,nftables,dnsmasq daemons are NOT in this
merge. In particular, dnsmasq's absolute-host lease hooks are excluded. No
systemd enablement, init, binder, guest, network rules, kernel/module, radio or
application action is part of these commands.

## Root preparation and preservation before the first command

The exact run root in JSON must be freshly created root-owned0700 under
/var/tmp. Root builds its protected config snapshot from the already reviewed
task configuration, retaining existing profile/repository/policy and only the
targeted libcap package.use addition. Never copy keys/secrets into the guest or
print them. Resolve/pin configuration symlinks and selected ebuild/eclass/Manifest
inputs and normal FEATURES before the separately reviewed actual dispatch;
a user-writable source config is not privileged execution authority. No
make.conf/world/profile mutation or --autounmask write is permitted.

Preserve current libcap through ordinary Portage quickpkg to the new protected
PKGDIR, with gpkg/signing and unmodified config included. Actual signature,
payload and installed-version/USE correspondence must pass before merge.
Privately preserve modified protected PAM files plus current account databases
and relevant VDB state under root-only access; evidence exposes hashes and
bounded identities only, never shadow/password contents. These are recoverable
inputs, not authorization for a blind rollback.

Record world/world_sets/profile inputs before and after. Merge is oneshot;
unexpected changes or extra resolver atoms/USE deltas stop admission. New
account names/IDs and existing libcap/PAM state must still match the reviewed
precondition immediately before actual merge. A partial merge stops and retains
Portage logs/VDB/artifacts; never automatically remove accounts or restore whole
account files over concurrent changes. Manager evaluates any repair separately.

## Exact resource, logging and result boundary

The JSON supplies root systemd-run Typeexec/wait/pipe/collect/literal argv;
quota8,affinity0-5/12-17,memory12GiB,swap0,tasks256,Nice10,
KillModecontrol-group and15second stop grace. taskset repeats the allowed
affinity but nice is applied only once. Phases bound120/60/1200seconds. Each
must settle before the next; no overlapping heavy job. Normal sandbox/userpriv/
signing FEATURES remain unchanged, gpkg is explicit, network fetching belongs
only to Portage's admitted source fetch path. Root's current signing authority
is reused without outputting credential material.

Each command uses env-i with fixed PATH and task HOME/config/cache/distfiles/
binpkgs/PORT_LOGDIR; both subject buses are nonexistent. No host display/XDG
session socket is inherited. Parent retains exactargv, PID/start/pidfd,
invocation/cgroup/current worker and actual effective caps/nice/affinity.
Preserve phase stdout/stderr and Portage phase logs; first unexpected exit or
resource/input mismatch stops without retry. Final launcher wait and unit
settlement are separate from package success, with no descendant-empty claim.

After merge inspect all8 VDB metadata/CONTENTS, exact USE/ABI, actual signed
packages and full verified payloads. Check account identity/default properties
without secret output; preserve PAM config-protect decisions and file changes,
world/profile equality and every phase result. This qualifies prerequisites
only. A fresh all13 build-only pretend and each missing runtime artifact,
full ELF/Python/GI/Qt/NSS/font/module stage closure, generated initramfs and
exact QEMU boot remain separately admitted steps.

## Source checks and requested action

JSON parses; static assertions verify eight atoms, three phases, one nice
application, allcaps, literalargv and no nodeps. No new supervisor framework,
package phases, account writes, compilation, VM or native action occurred.
Exact source hook pins and safe account-presence booleans are retained in
.cache/android-prerequisite-transaction; selected13 recipe pins remain9d0f.
Independent Platform review requested for this exact proposal. Root can then
grant protected input preparation/preservation and the bounded transaction
only when all preconditions are concrete; this source record is not that grant.
