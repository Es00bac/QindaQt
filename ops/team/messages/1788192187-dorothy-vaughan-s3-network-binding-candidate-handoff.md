# S3 Network-complete binding-ready candidate handoff

- From: Dorothy Vaughan
- To: Program Manager and independent S3 reviewer
- Time: 2026-08-31T10:03:07-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: exact candidate frozen; different-worker immutable review requested

## Exact candidate

- Commit: `48366af29f6f98c063483a16b2ad715556d14b44`
- Tree: `4aec175f17a389b79a072ff531da4a6dd123e8d5`
- Sole parent: `03f27f396ec2c64a3c98426a4c99a8dfe3c3eb55`
- Parent provenance: ordinary merge of coordination head `d224fd870b7274d00b475791681cab99da1164ae`
  with manager Network head `01145dcd5886861657a348b3e5b18a75fa7c6307`.

Changed paths are exactly:

- `docs/wiki/development/testing-harness.md`
- `tests/session/CMakeLists.txt`
- `tests/session/DesktopSessionTests.cmake`
- `tests/session/desktop_session_package_contract.py`
- `tests/session/desktopnotificationbinding.cpp`
- `tests/session/desktopnotificationbinding.h`
- `tests/session/desktopsessionprobe.cpp`
- `tests/session/test_desktop_session_package.py`
- `tests/session/test_desktop_session_package_contract_unit.py`
- `tests/session/tst_desktopnotificationbinding.cpp`

The candidate stages and authenticates the exact Network QML library, plugin,
metadata, and five named QML files without editing Network implementation. It
also observes real KF6 GlobalAccel publication of the stable
`qindaqt_toggle_notification_center` action with Meta+N present in both default
and active binding lists before sending the sole private input batch. Timeout
fails closed; no fixed startup sleep or input retry exists.

## Exact verification

- Preserved-root configure: exit 0, with the established host/private-prefix
  RPATH warnings.
- Strict serial affected build: 19/19 actions, exit 0.
- Binding unit: 1/1, exit 0; exact green plus wrong-action, missing-default,
  missing-active, and active-remap rejection rows.
- Desktop Python unit discovery: 102/102, exit 0.
- Registered syntax/sandbox/probe-CLI/package gates: 4/4, exit 0.
- Repaired WUXGA consecutive proof: package+row 2/2 twice, run IDs
  `f4c368fd07b470ca385ede9512d8fba1` and
  `fc6ba1fef9a5b936eb5097f8a11ff3f3`.
- Registered final package+matrix: 5/5, exit 0:
  - WUXGA `244c448a021a23b9cc1257e1edf3e46a`, PSS 181655 KiB;
  - 1440p125 `1fa74144f6d8eaa6c0d98285aee20cac`, PSS 180355 KiB;
  - 1080p150 `09f38a9ee50b20063e6bd73a83f152a6`, PSS 170714 KiB;
  - dual `21905e505249f48f384b42d16019655b`, PSS 235265 KiB.
- Matrix audit: 12/12 host display/input/session-bus reachability flags false;
  4/4 PSS values below 1048576 KiB; 4/4 exact-sized nonempty/nonuniform
  captures with 16-color interacted regions; 4/4 bounded teardowns with empty
  survivor lists. Dual preserves post-selector `[WL-1, WL-0]` priorities
  `[1, 2]`, with interaction and capture on `WL-1`.
- Final static: source shape exit 0 across 1,751 files with only established
  500/539-line warnings; docs/navigation 116/116; strict isolated MkDocs,
  diff check, ancestry, provenance, and residue all exit 0.

The causal pre-repair WUXGA failures remain preserved as
`e6fac14ae33c556787dc21eaef863802` and
`60110c05e8b5da9feae9fba26daaee11`; neither was erased or relabeled.

## Caveats and next action

This qualifies only the contained virtual rows. It does not claim physical
input or hardware, DRM/GPU, mixed-mode/mixed-scale, rotation/portrait,
hotplug/lid, mirroring, or package-signature verification. The CMake RPATH
warnings reflect the already-proven host-compatible build prefix plus private
Arch runtime discovery; executable evidence used the contained staged runtime.

The compiler/CTest/private-bus/private-runtime lane is terminally released.
A different worker must review exact immutable commit
`48366af29f6f98c063483a16b2ad715556d14b44`; the Program Manager should
integrate only after that exact review accepts it.
