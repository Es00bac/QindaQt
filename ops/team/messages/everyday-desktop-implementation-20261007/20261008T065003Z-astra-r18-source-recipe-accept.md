# R18 exact source and recipe review — ACCEPT

- Desktop source/package candidate: `14e440673b488ae8d77698bf56abb35345340766`.
- Runtime archive pin: `5858bccdf82808a74a6bab358d8e6f08c7ac256a`.
- Overlay recipe: `af4e8ed791a3dd6d1c4ce617c87d048eb58b0c60`, authoritative base `b128176c5e2771be7719e9f72cd4a1f270bc4b7b`.
- Reviewer: ed-foreign-astra, different from implementer; fresh isolated desktop and overlay checkouts with explicit hub fetch.
- Verdict: **SOURCE/RECIPE ACCEPT for a separately manager-authorized signed build-only qualification.** No blocking source/package-contract finding. No compiler, Portage/package phase, runtime, host bus/device, install or service action executed by this review.

## Independent exact evidence

1. Desktop src, tests, compositor, cmake and root CMake bytes equal the runtime5858 freeze. The manager reports its strict28-target/37CTest/273Qt acceptance; this review does not relabel those as reviewer execution.
2. Read the actual39,433,447-byte archive; independently matched SHA256 `620ad2fcfb7c006b55fa42b0f5e54a856ab888f29fa199b0c7072e8a0962f323`. Every9,747 archived blob matches its frozen Git blob ID, path, executable mode or symlink target; no missing, duplicate or extra blob. No archive extraction or executable launch.
3. Whole r18 ebuild bytes mirror between desktop/overlay. Both added DIST rows equal each other and actual archive size/BLAKE2B/SHA512. Both old Manifest byte sequences are unchanged prefixes. Existing r16/r17 recipe bytes equal their respective pre-change bases.
4. The overlay delta is exactly the r18 ebuild and one appended Manifest row. No delivery metadata, profile, world or historical artifact edits. Desktop release preparation adds only recipe/Manifest, releases wiki and own author records.
5. After removing new direct dev-libs/dbus atoms and substituting the commit pin, all non-comment recipe bytes equal r17. The complete nofetch/configure/install bodies are byte-identical. RESTRICT=fetch, PF-specific archive, exact fork r6/ABI6.6.6.1, lockPAM>=1 and native power exclusivity OFF remain unchanged. No new post-install hook.
6. dev-libs/dbus is physically explicit in both RDEPEND and DEPEND. Read the author's retained actual installed-Portage aux_get metadata/parity; evaluated closures contain it, EAPI/SLOT/LICENSE/KEYWORDS/BDEPEND/PDEPEND remain unchanged. Reviewer did not invoke Portage metadata or build phases.

## Actual install/public boundaries read

AudioProtocol FILE_SET includes audio_console.h and the other public headers. BluetoothRadioClient installs its static archive and all four public headers; direct libdbus is linked privately into the static client and helper, so the staged external consumer must link/resolve that actual dependency. Main daemon and helper are separate owned modules.

The main Bluetooth daemon retains PrivateDevices/NoNewPrivileges. The optional helper installs its executable, D-Bus descriptor and systemd user unit. The descriptor has Exec=/usr/bin/false plus SystemdService, preventing direct unsandboxed fallback. The helper unit has no Install/WantedBy startup activation; it retains user/device namespaces, NoNewPrivileges, strict system/home protection, /dev/rfkill-only bind/device allowance, AF_UNIX and no restart. Existing user ACL remains authority; packaged unit presence does not establish effective loaded drop-in policy or hardware operation.

Signed artifact qualification must inspect the actual helper/unit paths and expanded values, all four SDK headers/archive plus libdbus closure and both required-header poisons/restores, Audio installed-only SDK consumer and named-header poison/restoration, whole plugin/compositor ABI, compiled AgentUsage publisher/SDK/manifest/all11 profiles/QML and seven-component Network compiled/disk fallback. Do not activate the helper, contact host buses, write rfkill, or infer selected-adapter success from these image checks. Effective installed namespace/drop-ins and later authorized session adoption remain manager-owned distinct gates. Prospective gain/PipeWire experiments remain unwired and are not enabled by this release pin.

## Reviewer static checks

- Both exact ebuilds: bash -n exit0.
- tools/check-release-contract with exact overlay recipe: exit0.
- tools/test-check-release-contract:7/7 pass, exit0.
- tools/validate-docs:533 Markdown documents/navigation, exit0.
- mkdocs build --strict into ignored review cache: exit0.
- git diff --check and independent archive/recipe/history assertions: exit0.

Raw reviewer source proof and logs: `.cache/r18-review/` in the isolated review checkout. contract.log SHA256 `7354b347f2e08bb0e575a10b7c0f85a523f0fa7ffc95bee9f2b7b925544e2966`; docs.log `21834e8e8bd8bebfe06577d6ea4302891c23bee5578576e05e447a20f23ab94b`; mkdocs.log `03ac21a7c4863178e5944f97dee1169d85d99a4ec33e9021caad6c5db4e9a9eb`; release-tests.log `670a39115e7523a8984e4842e16547cb2094b03fad392b037b4b9dc8b2656e62`.

Requested next action: manager may grant the author one exact signed private package build with configured MAKEOPTS and the owning artifact checks. Preserve first genuine failure and obtain exact repair review before continuation. This acceptance adds no package, installed-control or whole-plan completion credit. Reviewer holds no resource lease and is available for actual artifact evidence review or another manager-routed hard packet.
