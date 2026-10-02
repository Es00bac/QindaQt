# Independent native-input historical receipt / NativeShortcuts source review

- Timestamp: 2026-10-02T19:36:24Z
- Reviewer: pf_capture_privacy_coverage / pf-power-delivery-20261002; different from implementer qinda_icon_brand_audit.
- Review base: bf0d6597c75c64950eb2f751aefc4ebd764af104, isolated qinda worktree on review/pf-native-input-receipts-sol-20261002.
- Decision: ACCEPT historical native-input handoff within its exact stated scope; ACCEPT NativeShortcuts source only. No blocking source/receipt finding.

## Exact objects

- Handoff: aab2e3297375c854b85f890315849faad8e29fa8.
- Desktop test: 2b36c431e5da98cfcb0d14d5da8f38fea97727a7.
- Desktop embedded production: e2f4c01e0ae2e720237b2f12c24219f005f66757.
- Runtime fork: 56a29e58668faca73fe7d8de46b2f2238fc158f9.
- NativeShortcuts fixture: 6cfbaa82e4a2dff707387efd569530baedfe205e, with ancestor d97623ff17e245339caf95b05faa9e867c67182c (git merge-base --is-ancestor exit0).
- Abbreviated e2f/56a did not independently resolve; these full receipt-pinned objects do resolve. 2b36 is ambiguous in the fork, so repositories and full hashes are normative.

## Genuine native path and admission

Read exact desktop fixture, ordinary clipboard client, private runner, fork driver, NativeShortcuts integration fixture/CMake and native endpoint/lifecycle sources. Desktop git diff e2f..2b36 -- src is empty. Native input fixture embeds the actual ResidentPortalService/PortalFoundationComposition and starts installed xdg-desktop-portal; public requests reach actual EIS/native privacy and lock transport. Its ordinary session caller has a separate bus connection. The ordinary Qt Wayland peer copies only after actual injected Ctrl+C and pastes actual QClipboard bytes, with exact remote/local payload assertions. Consent requires a mapped helper and actual clipboard choice. Real barrier activation, KEY_Q, Release/KEY_W exclusion, Close and lock revocation are asserted. It does not mock those transports or pixels; no capture pixel proof is claimed.

The AUTHON noninstalled fork driver input mode substitutes a deliberately nonexistent capture broker sentinel and verifies !testBrokerReady(); this denies capture publication rather than bypassing authority. No production admission/source changes accompany this fixture. Same compositor startup dumpability/core invariants are retained.

NativeShortcuts6cf corrects the original same-connection fixture by connectToBus with its own named connection/unique sender and asynchronous calls while the compositor event loop runs. It calls org.qindaqt.Shortcuts1 at /org/qindaqt/Shortcuts1, checks actual compositor bus ownership, and registers targeted activation/deactivation signals. Existing authenticated(message) checks a unique sender and bus serviceUid==geteuid; these tests do not alter it. Real compositor keyboard/layout, rolling sequence, repeat, native lock and ordinary focused-surface inhibition paths are covered in source; rows unregister transient bindings and disconnect the caller. Conditional CMake requires native shortcuts and test-only session-lock authorization, and does not install the fixture.

## Receipt verification

Read-only qinda hash replay of all11 referenced runtime/build/docs files PASSED. Archived status exit0/6.383063s and actual logs show fixture4QtPASS/0FAIL/0SKIP (2 behavior methods plus init/cleanup,2605ms) and actual driver3QtPASS/0FAIL/0SKIP (5541ms). The historic command/preflight bind the old executable/plugin/core hashes and task-private bwrap topology. This review did not rerun binaries or claim current mutable shared artifacts remain old56a. The empty build.log SHA is genuinely empty; separate commands/status records bind that build receipt. Ten previous failed/setup attempts remain explicitly nonaccepted in the handoff.

| Receipt path | SHA256 | Result |
| --- | --- | --- |
| build/native-remote-runtime-copy-input/command.json | `b69b1754431cfd56c8a6cebe38ba8ed649784d8fe47ea2816a3ee9151b2bd686` | match |
| build/native-remote-runtime-copy-input/status.json | `5ce9f8e1651586af1db08d9767a742055a33c6507d399eecf2d10d62f57aab7e` | match |
| build/native-remote-runtime-copy-input/runtime.log | `60878a71dfabaab132a151b603a30ce71f6446325523df49480ca3dca31f0cd7` | match |
| build/native-remote-runtime-copy-input/preflight.json | `0e8e4adb0275f9266aae6cceb5000342f4bd15a3302c8c2cfacbb7c72adc86ab` | match |
| build/native-remote-runtime-copy-input/evidence/compositor.log | `871601baa3aa52ba81c43587741f52da8e111e27f55fe579b70b37000421f4a5` | match |
| build/native-remote-runtime-copy-input/evidence/consent.audit | `82dc0471e6b0b5809133888426c6074bf4615f5453d186d1bf3dbecd4ca3c038` | match |
| build/native-reference-copy-input/commands.json | `d8c41308e167481d06b2fbbccf876a2a4985e06a32ee3ee1f5a9c43b6345fdf8` | match |
| build/native-reference-copy-input/status.json | `c9e5ceedf9dd822a2b805948f402dad78f89dda5041b00f2aafdd470752dcd5e` | match |
| build/native-reference-copy-input/build.log | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | match |
| build/native-docs-final-status.json | `9f53004d150187f620c88ab1968df614510a22f924cadd3aa1247ddd50a7ce0f` | match |
| build/native-docs-final.log | `441f130cf362c5df098405af92437e18d8643d885c80ab4fff757d3ac2ce8a97` | match |

## Scope and remaining gate

Olde2f/56a qualification is private offscreen/software native input, without host DRM/input/display/bus or installed desktop acceptance. Archived driver warnings include missing X11 socket/default cursor and applications.menu; they do not prove a visual shell or application catalog. InputCapture is frontendv1. No final native-name/PF21, Power/Workspace, combined ScreenCast sharing, Notify/restore, package or final release qualification follows from this historical pass. NativeShortcuts6cf is source-accepted only; no compiler/runtime was run in this review. Cleanup source uses owned child groups, but this receipt review does not add an independently measured historical starttick/no-survivor claim.

Changed paths are only this new immutable reply and the reviewer's live board; no product source or peer record changed. Next executable gate is manager-coordinated coherent final-source NativeShortcuts/native Power qualification on shared warm artifacts after exact composition, not replaying old qualification as current acceptance. Available for that bounded gate/help; no resource lease retained.
