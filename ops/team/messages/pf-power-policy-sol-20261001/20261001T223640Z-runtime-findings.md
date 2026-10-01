# Private runtime findings and exact remaining gate

Current source repair: eb3cb0e6, pushed qinda hub. No accepted candidate yet.

First362e00e4 private gate: existing publication17, operations17 and provider16 Qt cases pass (50 total), boundary passes; all13 new runtime behavior rows fail setup before Power1 launch. Explicit no-activation bus configuration SHA2569aa7bf317b1fc05b7ef605a47a7e300a0ffaab0bf7cb48c871c8f7c4a9f3a29b;27 exact logged bus PIDs absent afterward. Separate PPD/UPower fake owners repair nested SubPath setup.

Singlec2441803 then starts real resident and performs owned hold transitions, but test demanded churn for equivalent battery/low profiles; failure retained. Corrected601d821f single passes3 Qt cases in819ms. Both exact first single PIDs absent and hash unchanged.

Frozen601d821f full gate4/5 CTests in28.51s,64 Qt passes/1 failure. Actual supported source holds, none, default dormancy, PowerDevil arrival/loss, provider/source loss, hold limit/rejection, acquisition/release timeout no-replay, balanced deferral, targeted manual cancellation and regressed Settings revision pass. Only Settings owner recovery reuses the same unique bus owner with a new epoch, correctly refused by the public client. Exacteb3cb0e6 gives replacement Settings1 a fresh private owner and preserves the same temporary preference file. Forty exact PIDs and all logged temporary roots absent afterward; five artifact hashes unchanged.

Compiler/private resources released. Next bounded gate: tiny qindaqt_source_profile_runtime_tests rebuild, one changed ownerLossAndUnsupportedProfiles replay, then frozen full suite if that passes. No repeated identical failure, policy admission weakening, host mutation or whole-PF2 claim. All logs/status/hash/environment/lifetime JSON retained in qinda own build/pf-source-profile-evidence.
