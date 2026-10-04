# Newly installed r6/r13 contained greeter PASS

- Timestamp: 2026-10-04T17:03:47+00:00
- Worker: `/root/native_permission_repair`
- Status: available; private resource lease released, no owned survivor
- Source worker tree clean/unchanged at accepted fork `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`

One manager-authorized repeat against the newly installed pair passed **exit 0**:

```sh
ssh qinda 'python3 /home/cabewse/.cache/qindaqt-native-greeter-qualification/qualification.py --execute'
```

### Immutable input adoption

Reported old greeter pin before any edit; manager authorized precisely new r13 greeter SHA. Reported removed old Portage stage independently; manager separately authorized exactly IMAGE='/'. No broader override. Two-literal reverse substitution equals original4eab bytes; exact diff stored in ignored worker cache and qinda private cache `installed-r6-r13-pair-inputs.diff`.

- Before helper SHA: `4eabcedba5cb78fe195aca75875313092564a6ec19d269b141e906a31ea41543`
- Greeter-pin-only intermediate: `3dcaf73e7fe812f2bd9e61fe4afebdfbd2cdf1bb66d8f403a815f3f921771441`
- Final executed helper: `2daa9fa94d95fc7cd5e89ff6cb0513ce1714f3fd9bba54411c541f882a1ff301`

All source/core/metadata/production-role, PAM masking, UID/capability, bus owner, trace/state and cleanup predicates stay unchanged. The existing readonly `/artifact/usr` bind now aliases current trusted installed `/usr`. No software installed by worker and no production source/system setting modified.

### Installed provenance and exact hashes

Read-only installed package ebuild fields prove:

- `gui-wm/qindaqt-kwin-6.6.6_p1-r6`: source `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`
- `gui-wm/qindaqt-desktop-0.1.0_pre20261002-r13`: source `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b`

Readonly hash/stat checks and runtime input assertions preserve:

- Installed compositor: `e4690b828d267524caeef582928b2e53840e1a1ee99026fbd4c11301dc86aba1`
- Installed core: `d8f64e74d55a6c64b71e8bddbbbeb4db7282a0191ab2065eda1a9d3a12fe1b7d`
- New installed r13 greeter: `cacc3ae9263a2d71b37d0c30380ea2ff9850bfd8bba60f8bc2c4458c5e4d55b4`, root:root 0755/trusted parents (manager independently checked package CONTENTS on both hosts)
- Installed native lock metadata: `7586f5e76eb2da60b9f5d3bdcd307516fc3116061ef38821c158707edd6e0cd8`

### Runtime evidence and cleanup

qinda evidence:
`/home/cabewse/.cache/qindaqt-native-greeter-qualification/run-20261004T170001Z-4sw8h5a6/evidence`. Summary/bootstrap/source-and-images/server-only log/native supportInformation retained; run parent retains exact command/helper copy. Local ignored `installed-pair-evidence/` has JSON copies plus full installed-pair-provenance.json, also copied to qinda private cache.

RequestLock=true from owned private native service `:1.1`, pinned to compositor PID 5. Real installed r13 greeter is direct child PID 30, PPID 5, UID 1000, argv exactly `/usr/bin/qindaqt-lock`. Standard lock object 34 creates role 43 for wl_surface 38/output 23; server configures serial 9 at 1280x720, child acknowledges 9, attaches non-null buffer 47 and commits. Protocol locked and frame callback done observed. Locked/Protected=true remains with real child/role for 0.5 seconds. Native supportInformation reports QPainter.

Loader initializes `/artifact/usr/lib64/libqindaqt-kwin.so.0`, now readonly alias of installed accepted r6 core; file hash matches d8f64 above. Production executable/core bytes are unchanged from the prior screenlocker-ON/fixture-OFF r6 image, whose production configuration was verified before the first gate. Bootstrap again records original UID namespace, UID/GID 1000/groups empty, CapEff/CapPrm/CapAmb zero and NoNewPrivs=1. PAM fixed-image mask remains nonregular and inaccessible for execution; no credentials or authentication attempt beyond unavailable worker start can occur. No unlock, live endpoints, physical devices/session actions.

Only one namespace/compositor execution in this installed-pair repeat, exit 0. Compositor cleanup exits 15 after owned SIGTERM. Root readonly /proc scan restricted to exact owned run/helper/artifact argv markers finds **zero survivors**, including root wrappers. Private runtime resource lease released; no follow-up test or helper/source change needed.

Bounded caveats: qinda software virtual launch/standard role/frame feedback only; no PAM acceptance, pixels/physical scanout or live-session adoption claim. Parent owns deliberate desktop adoption and remaining incident closure. Concrete help offer: available to explain this exact retained evidence or answer reviewer/manager questions; no unrelated work claimed.
