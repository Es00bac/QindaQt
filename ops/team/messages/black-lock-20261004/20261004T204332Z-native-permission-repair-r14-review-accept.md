# Independent exact r14 recipe and source archive ACCEPT

- Timestamp: 2026-10-04T20:43:32+00:00
- Reviewer: `/root/native_permission_repair`, live collaboration agent distinct from recipe author/manager
- Verdict: **ACCEPT**, no source/recipe blocker or candidate change
- Overlay commit: `b6d14354f48081c70b5ee50168d4debdc778be98`
- Overlay tree: `f020b8e9f398fd8af2bbc91f073c935012b418cf`
- Overlay parent: `a810eeb0b902ab66d4d451661a330307cbb76dcf`
- Source commit: `a7b302aec8255738d3f6dcd0e3f0a643e05d5ae2`
- Source tree: `0d98148cd1eedf64d989ae071f4a99ebc73fd65a`
- Isolated clean detached qinda review: `/home/cabewse/work_SPaC3/QindaGentoo-keyring-reconnect-review-20261004`

Inspected all 121 lines of the new `gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r14.ebuild`, its exact parent diff, overlay instructions, relevant source CMake/public SDK boundaries and complete archive. The commit changes exactly that new recipe and one `gui-wm/qindaqt-desktop/Manifest` row. It preserves 559 existing non-Manifest tracked paths byte/mode-identically, all 115 prior Manifest rows (116 now), and the 36 approved delivery atoms unchanged; delivery still selects r13. Compared with r13, only the source pin and two explanatory comments change. All 13 configure arguments, RDEPEND/DEPEND/PDEPEND/BDEPEND, relative libexec setting and ordinary full CMake configure/install phases are unchanged. No signed-base overlay, new lifecycle hook, policy setting or privilege operation was added.

## Archive and source identity

Archive `qindaqt-desktop-0.1.0_pre20261002-r14.tar.gz` in manager cache and `/var/cache/distfiles` independently hashes identically:

- Size: 38,830,888 bytes
- SHA256: `0646fc2cd5480ac126b80cf420673663d1bf28bd5d1c23d148ec8a264f886cf4`
- BLAKE2B: `48fecded5482dd12fd216c2b9fa57e755ede71a5f420bcd7235b5fe6f3158e4b45d7751e5c22650f6d8c69eec84daef5bea333c4c0ab152c0b1887978eac2fa1`
- SHA512: `10155ca055f59e8a0da4b7747021b8f3a86d28ee0aa337b8a28129530bb8999eef12e056d54a3e37bd681f84f4f34cd840ba4ab2fa513ecf26ab222061d8bbd8`

Manifest size/BLAKE2B/SHA512 match. Independent bare-hub `git archive --format=tar --prefix=QindaQt-a7b302aec8255738d3f6dcd0e3f0a643e05d5ae2/ a7b302aec8255738d3f6dcd0e3f0a643e05d5ae2` matches the complete decompressed source tar byte for byte: 85,893,120 bytes, SHA256 `4cbb779bb5e7159e3152cad25e84ea663195f864e147d558eb0c8b28c4e2633b`. Applying system `gzip -n -c` reproduces the published compressed archive exactly. Git's built-in gzip on this host creates a different compressed container with the same tar; this is not a source mismatch and does not justify recutting a published archive.

All 10,991 tar members are unique, safe relative entries under the recipe's exact S prefix: 9,266 regular files + 1,725 directories, 0 symlinks/specials. The 9,266 files exactly cover the Git source inventory; no submodules/export omission. Pax commit comment pins a7b302ae. Source descends from independently accepted f2f2d392, exact assigned eeed1f6c and deployed r13 ab7fc8f3. The f2f2→a7b3diff contains 40 docs/ops paths only: every production source/build/test byte is retained. The root's Office D-067 paragraph and reconnect ADR 0348 link are present.

## SDK and install safety

Exact dependency `=gui-wm/qindaqt-kwin-6.6.6_p1-r6:=` parses through installed Portage and is retained in RDEPEND and therefore DEPEND. Retained r6 recipe pins accepted 24d0c6a6; fork ancestry preserves deployed dd74 shortcut repairs. The source's plugin/controllers retain EXACT 6.6.6.1 SDK selection. Both installed core/decoration SDK version files report 6.6.6.1 and, with the authority launchpaths header, match the r6 package CONTENTS (3 files). All 4 frozen capture executable/desktop paths match `/usr/libexec` and `/usr/share/applications`. The standard cmake eclass supplies `${EPREFIX}/usr`; unchanged recipe `KDE_INSTALL_LIBEXECDIR=libexec` and source's matching installed paths preserve the existing fixed capture admission contract. This was static/read-only verification; no CMake configure or install performed.

## Exact evidence

All commands ran on qinda, no laptop build/test/GPU load:

- `python3 ~/.cache/QindaGentoo-keyring-reconnect-review-20261004/review_provenance.py`: **exit 0, 29/29 checks**
- `python3 ~/.cache/QindaGentoo-keyring-reconnect-review-20261004/review_sdk.py`: **exit 0, 6/6 checks**
- Embedded `bash -n` recipe syntax and `git diff --check`: exit 0
- Exact review HEAD/hub refs/clean identity:pass; no source edits or compiler/runtime resources were created

These 35 checks are packaging/source/static provenance checks, not additional CTests or authentication claims. Two initial cache-helper attempts exited 1 due reviewer assumptions:counting 14 configure flags instead of 13, and expecting Git-internal gzip bytes to equal system gzip-n. Both original logs are retained; corrected independent evidence above passes without candidate changes.

Qinda evidence archive: `/home/cabewse/.cache/QindaGentoo-keyring-reconnect-review-20261004/r14-review-evidence.tar.gz`,8 members, 6,733 bytes, mode 0600, SHA256 `a9736d3879f33406ef05a6ed2ad66f32560dd288741371dfc0aceacfd753d3f6`. Provenance helper SHA256 `a6f321656ba29686c7c31e21baa839177a53a6b0988e676fb9b9fea5bb529062`; SDK helper SHA256 `5c86e0eb13aa069c9b1ec4155c8770ebbabf2923a5ea8ceaa5f690ef3cd68962`.

## Next boundary and help offer

Root may proceed to the exact r14 full signed Portage build on qinda. Signed artifact/source-image acceptance, binary adoption on both hosts, future-login supervisor adoption and live credential/server authentication remain separate manager/user gates. This review performed no build, live bus/API, install, signing-key, app/service/desktop/config/credential or speech operation. Existing desktop stays open. Available for bounded independent exact signed artifact/content provenance review after build; no unrelated product outcome claimed.
