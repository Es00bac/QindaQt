# SOURCE ACCEPT / PRIVATE-NATIVE ACCEPT — Audio density candidate d050c1f3d3ee017db9e535be642c121131e45ba5

- Timestamp: 2026-10-08T11:36:08Z
- Reviewer: GPT Audio density reviewer
- Candidate: d050c1f3d3ee017db9e535be642c121131e45ba5
- Native-executed product/test ancestor: 874e6ec61c87dac57eaaeeca3a432f6df76f7dd1
- Source base: 40d7f209661a9257812aca96503a3d9675cb81f3
- Prior SOURCE NEEDS_FIX receipt: review record commit 0751fef9664be810aa53dd60437566173d37b09f, exact ancestor 874e6ec61c87dac57eaaeeca3a432f6df76f7dd1
- Review branch: review/everyday-audio-density-repair-20261008
- Review worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-audio-density-review-repair-20261008

The sole blocking finding is resolved. ADR-0362 supersedes only ADR-0288's Settings/compact-page presentation clauses, preserving the historical text and its service/schema/persistence/readback/bus-delay decisions. Reciprocal links, ADR index and MkDocs navigation are present. The owning latency evidence now names Mute → Details → field and the 64-logical-pixel gate. ADR-0362 remains Proposed for manager acceptance with reviewed integration. The prior source/boundary/decomposition review stands because product/tests are byte-equal to the reviewed native ancestor. No new blocking finding.

Independent repair and evidence checks:

- `git diff --exit-code 874e6ec61c87dac57eaaeeca3a432f6df76f7dd1 d050c1f3d3ee017db9e535be642c121131e45ba5 -- src tests`: exit 0. `git diff --check 874e6ec61c87dac57eaaeeca3a432f6df76f7dd1 d050c1f3d3ee017db9e535be642c121131e45ba5`: exit 0.
- `python3 tools/validate-docs` in the exact review worktree: exit 0; 534 Markdown documents and MkDocs navigation. Strict MkDocs result remains root-owned.
- Archive verifier: exit 0; 892,922 bytes, SHA256 `fa51cf00fcd7d7f0a4cfd4aeef78a7ae537fe82d663a0e1805d53ec6cf583b20`, 34 regular members and exactly 33 indexed payloads. Every size/hash matches both archived and extracted bytes; no symlink or unsafe member. All 11 source entries match d050c1f3d3ee017db9e535be642c121131e45ba5; all eight PNG size/hash/dimension entries match. Local candidate images match the archived index before independent visual inspection.
- Preserved actual native build: exit 0/18.092 s for seven requested owning targets. Raw build log shows changed QML/density/latency/page objects rebuilt. Read-only build cache/rule inspection confirms Debug, strict warnings enabled and `-Werror` on all seven targets' object rules. No compiler was executed by this reviewer.
- Preserved actual CTest: exit 0/17.286 s, all nine rows pass in raw text and XML. Full LastTest contains seven Qt summaries totalling **57 passed, 0 failed, 0 skipped, 0 blacklisted**; wheel's own full summary is **19/0/0/0**, without relying on its truncated JUnit output. Individual case lines retain public default/volume/mute/retry, stale/owner-loss, admission fallback, paging, latency/read-only/unknown, wheel/readback/refusal/no-replay, routing, console and both-DPI disclosure/snapshot/replacement coverage. Wheel has four private AF_NETLINK-denied QWARN lines; this is not a warning-free claim.
- Actual unchanged-production negative control `b4d587c4f54b2255f63bfcb3e8fe5ff1dcfb30c3`: `git diff --exit-code 40d7f209661a9257812aca96503a3d9675cb81f3 b4d587c4f54b2255f63bfcb3e8fe5ff1dcfb30c3 -- src` exit 0; preserved build exit 0; native control exit 2, **2 pass/2 fail/0 skip/0 blacklist**, two observed 74-pixel pitches fail the 64-pixel gate.
- All four candidate capture commands exit 0. Independently viewed immutable 900×720 and 420×320 captures at normal/2× scale after hashing: names/default/volume/mute/Details readable, no common-control overlap or clipping; both output and both input common controls fit in the compact initial viewport. Source/runtime tests require at least 22-pixel slider height and positive spacing; advanced controls retain scrolling and keyboard access.

Changed product paths across the candidate: AudioDeviceSection.qml; focused density/latency/general page tests; Audio Settings wiki; ADR-0288 supersession metadata; new ADR-0362; ADR index; MkDocs navigation; author replies. Reviewer writes only its own stable record and two new timestamped replies.

Bounds: exact private-source/rendering review only. The global source-shape gate still has 63 errors on 61 unchanged paths, proven base-equal in the prior receipt; the changed AudioDeviceSection's 318-nonblank-line decomposition warning was reviewed, below its 350-line ceiling. The existing count-based row/aggregate gesture and per-channel delegate-refresh limitations remain unchanged. No native rerun, Portage, host bus, live/physical device, Wine, Android or speech operation was performed here. No package, installed Settings, acoustic, volume-popup or whole-ED09 acceptance is claimed.

Requested next action: integrate this exact accepted candidate with ADR-0362 marked Accepted and the index synchronized; preserve root strict-doc and global-shape results, rerun affected integrated gates, then qualify packaging and installed usability separately. Minor nonblocking editorial follow-up: the compact-section sentence saying keyboard traversal is unchanged should instead say existing object/accessibility names are preserved and traversal includes Details. Compatible help offered: exact integration-difference or preserved installed-proof review without taking another owner's compiler/private-runtime lane.
