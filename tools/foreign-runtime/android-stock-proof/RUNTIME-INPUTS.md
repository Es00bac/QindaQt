# Selected stock runtime input plan — source only

This plan extends the source-accepted f676 fixture. No package phase, merge,
stage, initramfs creation or VM has run here. The guest remains stock
Waydroid1.6.3, fixed Vanilla images and the installed Portage kernel6.18.48.

## Exact resolver result and one next action

A private PORTAGE_CONFIGROOT references existing policy read-only and adds only
`=sys-libs/libcap-2.78 static-libs` in its own package.use. The keyword policy
`ACCEPT_KEYWORDS="amd64 ~amd64"` matches the previously reviewed runtime
pretend. Global configuration, world and profile are unchanged. Repository and
configuration inputs must be re-pinned before a separately granted build.
The first attempt without that keyword policy refused Waydroid; preserve it.

Normal pretend resolves 13 packages:12 new and one libcap rebuild. The existing
policy also selects ABI_X86=32 for libcap (shown as a change), so this is not
represented as a static-libs-only resulting package. runtime-inputs.json
freezes exact versions, recipe hashes and observed USE rows.

Waydroid alone with --buildpkgonly resolves just one package because it has no
build-time runtime dependency closure. Explicitly requesting all13 with
--buildpkgonly refuses because build dependencies are not merged. Neither
pretend is an executable runtime closure or a package success.

**Next action requested:** independently review and separately authorize the
eight exact prerequisite atoms below through ordinary Portage --oneshot,
with the task-local policy, signed buildpkg outputs,8jobs/load8 and the
manager's existing resource envelope. Re-run the frozen pretend at admission;
refuse any additional package or USE delta. This is a proposal, not a merge
grant. No --nodeps, fake VDB or extracted libraries in ambient compiler paths.

- net-libs/libnftnl-1.3.2
- sys-libs/libcap-2.78 (static-libs plus the observed ABI selection)
- acct-group/dnsmasq-0-r3 and acct-user/dnsmasq-0-r3
- acct-group/lxc-0-r2 and acct-user/lxc-0-r2
- dev-libs/libglibutil-1.0.82 and dev-libs/gbinder-1.1.52

These are the source-derived missing DEPEND prerequisites for the complete
selected set, including account eclasses. They change Portage-owned library/
account state and require explicit root approval. They do not merge/start
Waydroid,LXC,nftables or dnsmasq. After they are qualified, perform the exact
all13 build-only resolver again and separately build/package the remaining
five atoms; no build-only success is assumed in advance.

An alternate ROOT alone was rejected as an isolation shortcut: dnsmasq2.93
pkg_preinst/postinst access absolute /var/lib/misc/dnsmasq.leases, while nftables
has a ruleset preinst probe and a postinst saved-rules file. No such hook runs
during extraction. A fresh complete build sysroot would require another full
toolchain/root closure and is not the shortest current fixture path.

The readable local binary-package index advertised none of the principal
missing runtime/libcap packages examined. This is not proof that no remote
compatible artifact exists. Prefer an exact compatible signed package if the
manager identifies one; verify its signer, USE, ABI, metadata and payload
before replacing a proposed source build. Do not fetch implicitly during
source review.

## Stage inputs and signatures

All new artifacts require full Portage gpkg signature and payload verification
against the already admitted signer. Manifest hashes identify fetched source;
the observed package USE has verify-sig disabled where optional, so no upstream
signature assurance is invented. Existing installed tools/kernel may contribute
only separately admitted VDB-corresponding public files, with exact size/hash/
version and package provenance. No live directory tree is blindly copied.

The fixture root combines those admitted selected payload files with
prepare_overlay.py's exact generated configuration. This is package extraction
for a disposable test artifact, not host software installation. No package
postinst, unit enablement, service start or guest initialization occurs during
stage construction. Root selects an accepted desktop gpkg containing qindaqt-wm
and a compatible qindaqt-kwin; installed qinda desktopr13 is not silently
substituted for an accepted newer package.

Required full closure before inventory admission:

| Domain | Required proof |
| --- | --- |
| ELF | For each executable, shared object and plugin, inspect ELF interpreter, DT_NEEDED and RPATH/RUNPATH from staged bytes; resolve entirely in the guest root and recursively bind every dependency. Never run ldd on an unadmitted payload. Include glibc loader/NSS files without host NSS configuration. |
| Python3.14 | Exact interpreter, stdlib and python-exec/shebang targets; dbus-python1.4.0-r1, pygobject3.56.3 and gbinder1.3.1 extension/module files, their ELF closure and selected GI typelibs/GObject libraries. No import-based host execution to guess closure. |
| Waydroid/LXC | Whole selected stock Python/data/config/scripts and LXC commands/helpers/config; their actual shell commands including ip,nft,dnsmasq,mount,umount,modprobe and core utilities. Include the package-owned nftables variant of waydroid-net.sh. No host /etc. |
| Qt/KWin | Admitted desktop/KWin package pair, complete QML imports, Qt/KDE platform/render plugins and their ELF closure, fontconfig public data plus selected fonts, software rasterizer requirements. No GPU/DRI exposure. |
| Kernel | Exact6.18.48 kernel and matching modules, module metadata and recursive dependencies for overlay,loop,ext4,bridge,veth,tun and the stock netfilter/nftables rules. Built-ins must be proved from that exact config; no custom kernel or host modprobe. |
| Generated | Exact init/guest/windows/scenario and generated passwd/group/NSS/hosts/font configuration, fresh guest machine-id. Add any required service account rows from selected package metadata, not host account databases. |

The above is a mandatory closure checklist, not a claim that a complete stage
already exists. Artifact hashes for missing packages, all ELF/GI/QML resolution
and generated initramfs remain open. Stop on the first missing input; do not
test boot by repeated speculative asset additions.

## Executable pre-archive gate

boot_plan.py now takes prepared-root plus a separately qualified schema1
inventory. Every archived path must have exactly one object record binding
kind, mode and a nonempty input provenance identifier, plus file size/SHA256
or exact symlink target. Directories also require provenance. Any unlisted or
missing object, changed byte/mode/target, missing required executable/directory,
guest symlink escape/cycle or unsupported object refuses before cpio argv.

Required symlinks resolve within the guest root, including absolute links;
host paths are never used as link targets. Regular input reads use nofollow/
nonblock held FDs, observed-size bounds and version/name rechecks. This gate
does not authenticate the inventory's issuer/signatures and does not promise
atomic directory snapshots against privileged concurrent writers. The manager
must protect the stage and input inventory, recheck before/after archiving and
bind the archive listing and resulting digest before a separate QEMU grant.

No rendered input, resize/close, authenticated app identity, platform origin or
ED completion is qualified by these source/pretend/tiny-file tests.

## Laptop alternative — no prerequisite merge assumed

The fixed laptop profile reuses the installed stock Waydroid1.6.3/LXC7.0.0,
nftables1.1.6,dnsmasq2.93,gbinder1.1.52 and Python bindings1.3.1, with the
installed R20 compositor/owned KWin and matching kernel6.18.48. A read-only
inventory records exact VDB CONTENTS and metadata hashes, source MD5
correspondence, SHA256/size/version-pinned public files, resolved ELF
interpreter/DT_NEEDED dependencies, selected matching kernel-module dependency
closure, Python/GI/Qt plugin/QML data and fonts. No host /etc,/home,/run or
private state is an input. Installed provenance is not relabeled as a new
signed binary package.

`laptop-inputs.json` binds the exact review evidence rather than copying
thousands of host-specific observations into a general public API. All
unresolved inputs must refuse the actual stage. The inventory is a selected
static closure candidate: script/dlopen/QML runtime behavior, guest memory
sufficiency, actual stage admission, boot and two-window behavior remain
unqualified. The first collector variants and their usr-merge/native-ABI/GI
selection corrections are preserved; zero final static issues is not runtime
success. The actual protected stage must pin generated overlay sources and
use the existing `boot_plan.plan`/`stage_inventory.admit` gate unchanged.

The laptop protected stage now has a narrow owning copy_stage.py source.
It consumes the exact installed-input inventory plus separately protected
helper/plan pins; the manager must admit this candidate before a stage grant.
Current-source reads and synthetic controls are not input materialization.
The fixed expected hashes and VDB witnesses survive copy normalization, and
the generated complete stage is checked by existing stage_inventory and
boot_plan before any separately granted archive or guest operation.

The first actual stage using the predecessor inventory (6726 objects) failed final stage
admission after copying653,337,381 regular bytes: libgbinder.so.1 targeted an
omitted intermediate libgbinder.so.1.1 link. That failed stage is retained.
The corrected read-only collector uses input_links.chain to retain every
VDB-owned hop (maximum40, explicit cycle/public-root/type/version refusal)
rather than Path.resolve() collapsing directly to the terminal ELF. Its fresh
inventory adds exactly four intermediate symlinks and preserves every original
regular-file record, byte total and172 package witnesses. A Linux doubled
leading slash is canonicalized before explicit usr-merge mapping; unowned
fallback resolution has been removed. No admission guard is relaxed.

The owning copier changes only its fixed expected inventory count6726->6730.
The new manifest, proof and fresh protected-stage plan require independent
review and one new grant. The existing partial stage must not be reused or
patched in place. This closes an input-selection source defect, not actual
stage or guest qualification.
