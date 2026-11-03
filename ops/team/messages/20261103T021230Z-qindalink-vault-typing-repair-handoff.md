# QindaLink vault-typing repair handoff (review of e643c31b5)

Merged main (LAN mode, packaging prep) first; conflicts were additive (CMake lists, main.cpp options, docs index, run.sh).

## P1
1. Whole-fact target: Token and Job carry appId and title; start check and every tick compare uuid, revision, appId, title. Tests: titleChangeMidJobAborts, appId and title cases in focusChangeBetweenConfirmAndType.
2. Heartbeat: relay republishes every 1 s (refresh(true)); FocusStore::current() is empty when older than 3 s (injectable clock). Tests: staleFocusIsUnknown (fake clock, also mid-job), relay heartbeat test.
3. Hooks: --trusted-caller, --trusted-shell-caller, --test-portal, --test-open-gate, --lan-port, --lan-test-address, --pairing-window-ms exist only under QINDAQT_LINK_TEST_HOOKS (option, default = BUILD_TESTING). main.cpp compiled without the define: none of the option strings present (checked with strings). Tests and run-cross-*.sh use the hook build (build/vt).
4. Cadence: vault TextTyper at 2 symbols per 8 ms (TextTyper::setPacing), guard before every tick. ADR-0440 states the residual risk. Test: secretCadenceIsSlow.
5. CallerPolicy test clientIsNotShell (fake inspector).
## P2
desktop.focus needs vault only (test focusRequestNeedsTheVaultCapability); wording says only keysym buffers are zeroed and Java Strings cannot be; windowFacts moved to runtime/taskwindowfacts.{h,cpp}.

## Evidence (build/vt, targeted targets only, ccache)
ctest qindaqt.link-{vault,vault-service,caller-policy,caller-dbus,caller-coverage,input,macros,messages,messages-dbus,service-smoke} and qindaqt.shell-link-focus-relay: all pass. phone/engine-tests/run.sh passes (VaultTypeTests 50 checks, LanTest ok); run-cross-vault.sh 3 passed. mkdocs --strict and validate-docs pass. Shell: the five touched objects (linkfocusrelay, taskwindowfacts, tasklistappletcomposition, shellruntimeapplication, shellruntimeapplication_applets) compile; the full qindaqt-shell was not relinked after the merge (PM build rule), and the earlier pre-merge link succeeded.
Not run: link-lan rows (targets not built here), APK build (no phone change in this round).

## Caveats
A 1024-symbol secret now takes about 4 s. The relay heartbeat test waits about 2 s.
