# Independent exact6ca compiler/source checkpoint

- Time: 2026-10-02T03:17:33Z
- Review candidate: **6ca8638893ba2715b5ab49612a0237e42725445b**; no final ACCEPT until own runtime gates.
- Author and reviewer are different workers; source/test/docs inputs remain exact6ca. Own metadata branch starts at exact6ca.
- Own changes only: ops/team/workers/pf-native-lid-recheck-sol-20261002.md and own new message directory.
- qinda evidence root: /home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-native-lid-recheck-sol-20261002/build/review-evidence; own fresh build/review-dev.

## Fresh compiler evidence

- Accepted productionOFF stage owner68c4d74f903b7e8990dd5fd5d509ec8154eac1d1 used read-only as CMAKE_PREFIX_PATH; KDE_INSTALL_LIBEXECDIR=libexec. Debug/shared/testing/strict ON; KWin plugin/host uinput OFF; /usr/bin/ccache launchers.
- `configure` actual argv `["cmake", "-S", "/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-native-lid-recheck-sol-20261002", "-B", "/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-native-lid-recheck-sol-20261002/build/review-dev", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_SHARED_LIBS=ON", "-DBUILD_TESTING=ON", "-DQINDAQT_BUILD_KWIN_PLUGIN=OFF", "-DQINDAQT_ENABLE_STRICT_WARNINGS=ON", "-DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF", "-DCMAKE_PREFIX_PATH=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-manager-capture-production-20261001/stage/usr", "-DKDE_INSTALL_LIBEXECDIR=libexec", "-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=/usr/bin/ccache"]`: exit0, 45.386s; PID597137/starttick36543160/PGID597137; minimum MemAvailable14222876672bytes; 23 observed lifetimes, 0 remaining. Log SHA25667e423b51f5ee32fb72585c43b7f1e646515dd654d1280d2d74012452d3c9bd7.
- `build` actual argv `["cmake", "--build", "/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-native-lid-recheck-sol-20261002/build/review-dev", "--target", "qindaqt_lid_runtime_tests", "qindaqt_critical_battery_runtime_tests", "qindaqt_session_actions_tests", "qindaqt_source_profile_runtime_tests", "qindaqt_power_profiles_adapter_tests", "qindaqt_power_service_operation_tests", "qindaqt_power_service_publication_tests", "--", "-j8", "-l24"]`: exit0, 50.571s; PID598974/starttick36547700/PGID598974; minimum MemAvailable11094052864bytes; 2 observed lifetimes, 0 remaining. Log SHA2568af33df639c8cc633e13d62de4cd0e7efcd92b7f6131b622808391cea9cdefba.
- Build exit0; final logged Ninja progress193/194 (dynamic denominator, do not infer194 completed actions). Seven requested targets and normal/private resident dependencies built; no author build reused.
- Core limit0, own new sessions/PGIDs, 3GiB30s memory guard, configure180s/build600s phase bounds. No safety stop.

## Independent source/static result

- Zero confirmed P0/P1: reviewed subscribe-before-query, authenticated same-user current Session1 owner UID/PID, production root login1 GetSessionByPID and typed User/Active, genuine-open arming and closed presence proof, FD generation/fresh read-through owner+Active, cancellation/no replay, own descriptor duplication/CLOEXEC/cleanup.
- Production UID0 is fixed in normal main; same-assembly UID injection is scoped only to distinct noninstalled test target. Existing frozen source-profile/critical/public SessionActions/Power1/XML/Settings/sleep/supervisor paths and old runtime assertions compare unchanged againstf951. Unsupported screen-off deliberately retains no FD.
- Shutdown fixture calls terminate and waitForFinished before peer EOF. This is actual process-shutdown/all-owned-descriptor cleanup, not graceful-live policy teardown. Other loss/late rows test live revocation separately.
- `docs` argv `["tools/validate-docs"]`: exit0, 0.515s, log SHA256b302faf22e6004c3527a606cf0196ac7ecb10d713b0d2ff7f29da1458b01113a.
- `mkdocs` argv `["mkdocs", "build", "--strict", "--site-dir", "build/review-evidence/site"]`: exit0, 8.280s, log SHA256f1fef3ce51d5bad09a72ab11b95dcfae09b8b393849bfab9a98770a82fe5a7b4.
- `boundary` argv `["cmake", "-DSOURCE_ROOT=/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-native-lid-recheck-sol-20261002", "-P", "tests/services/power_service/check_boundary.cmake"]`: exit0, 0.032s, log SHA25661757e4d66fce394f04b7dac7527628ef7c0195bd6a9a32744b30368caa471f2.
- `frozen` argv `["git", "diff", "--exit-code", "f951bffcb499d1a0b191ebc4a599ea61b18c8f43", "6ca8638893ba2715b5ab49612a0237e42725445b", "--", "src/services/power_service/src/source_profile_policy.cpp", "src/services/power_service/src/critical_battery_policy.cpp", "src/services/power_service/src/adapters/native_profile_authority.cpp", "src/services/session_actions", "src/services/power_protocol", "data/settings", "src/services/power_service/data", "src/session/native_sleep", "src/session_supervisor", "tests/services/power_service/tst_source_profile_runtime.cpp", "tests/services/power_service/tst_critical_battery_runtime.cpp", "tests/services/session_actions"]`: exit0, 0.008s, log SHA256e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.
- `diff` argv `["git", "diff", "--check", "f951bffcb499d1a0b191ebc4a599ea61b18c8f43", "6ca8638893ba2715b5ab49612a0237e42725445b"]`: exit0, 0.016s, log SHA256e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.
- Documentation validator reports487 Markdown/navigation; strict MkDocs PASS. No product/docs/assertion edits made.

## Frozen artifacts and bounds

- status.json: SHA25682bcd51608241e561329870ce33cd8ac69e2ffaaa98cf3de88a24d57beeb856e.
- static-status.json: SHA256649fbe99eea8409d604cc51d3656d845b45162adee3b51f136c69c5672bc6f09.
- artifacts.json: SHA2560c03c9b47ee7d59ec3f076939996e69832ac857af4af9499773d6ba63940f675.
- source-before.json: SHA256af80ade715ac1b2c182d6daa0c2184aec7fc22ef3ec4e9db719ab27540e242c7.
- source-after.json: SHA256af80ade715ac1b2c182d6daa0c2184aec7fc22ef3ec4e9db719ab27540e242c7.
- Before/after manifests identical across tracked src/tests/docs/tools/build inputs;13 own output hashes comprise cache,nine resident/test executables and three generatedMOCs.
- Sampled25 configure/build PID lifetimes absent; final owned PGID inventory checked separately. No private runtime, host bus/hardware/lid/inhibitor/power/PAM/display/device action.
- This review is bounded PF2 native lid evidence only; screen-off/full matrix/idle/final legacy retirement/installed hardware qualification remain separate.

## Resource release and next action

- Sole qinda compiler lease RELEASED after build; no compiler/private resource held.
- Request manager route separate sole private lease. Next authorized gates: firstGenuineClose:suspend then shutdownClosesEveryOwnedDuplicate, followed unchanged full10CTest137Qt with dead ambient addresses, private HOME/XDG, explicit untyped no-include/no-service-dir brokers and process/source/artifact cleanup audit. Expected normal production UID0 negative must execute.
- Read Platform queue and peer handoff; available for bounded exact reproduction/help within this review only.
