# Unchanged QtTest production build constraint handoff

- Time: 2026-10-02T00:35:27Z
- Authorized outcome: ordinary read-only build preparation for unchanged `testNativeCaptureAuthority` against exact manager production68c; no runtime, mounts or auth redesign.
- Prepared on qinda: `~/.cache/pf-capture-consumer-review-sol-20261002/build/production-driver-recipe/README.md`, executable read-only `audit_build_boundary.py`, full original compile/link argv, exact source copy, production/original needed-symbol lists, build-boundary JSON and logs.
- Command: `python3 ~/.cache/pf-capture-consumer-review-sol-20261002/build/production-driver-recipe/audit_build_boundary.py`; actual expected **exit2**, reporting concrete build constraint. No compiler/moc/link/native process was executed.

Exact68c source/CMake and original447 compile/link commands establish the native target is conditional on `QINDAQT_CAPTURE_AUTHORITY_TEST_AUTHORIZATION` (integrationCMake281/331–340). The unchanged source calls `WaylandServer::setCaptureTestPrograms` and `CaptureAuthority::testBrokerReady`; declarations/implementations require that macro. Manager production cache has capture testauthorizationOFF and BUILD_TESTING=OFF. Direct production ELF defined exports contain neither required function; original447 ELF contains both. Production also has no `libKWinIntegrationTestFramework.a`.

There is no valid one-TU/moc/relink command meeting both unchanged-source/original-flags and this actual productionOFF-library identity. Dropping the macro fails compilation of the calls; retaining it requires missing link symbols. MOC regeneration/RPATH changes cannot supply those functions. Relinking the old test archive/core or inserting test definitions would not prove the requested productionOFF artifact and is not performed.

Direct identities:

- Driver source68c=447 SHA256 `84c3a11442f650020adb12e0b8b11387349d8c8070d888964ad06651528e7d67`.
- Production68c core SHA256 `387bfefb1fcbcd12f9e881f8838ad81b1ecaae53f16bd56fd5e1d8873c6e7464`.
- Original447 core SHA256 `c99c0851ceeb9faa2e4ac2507e62285aed369cfb1ce8275a695bb8834abdf01a`.
- Original test-framework archive SHA256 `afdd5921b0ebe209f771c9135e04f04d9f96b27b53d46d4574c33869cb4be06f`.
- Audit recipe SHA256 `04925f2bafdf6f98e1175221a46ccd22773b8d9ffa69b711f6a657c1ba83dfc3`.
- Original compile/link argv text SHA256 `a379468e4e57d76c17973ef2af16cdf0fa223bc4ff9cc697d6e4ea7caaf9de54` / `d2d6b038862c3f1f98e79ee79610e16abdc28baeaf1b5a5d7a4b27114d6f328a`.

One audit helper Path-expression setup failure is retained separately, corrected before final exit2 constraint evidence. No attempted compile/link failure or native result is inferred.

Requested next action: manager choose the existing ordinary noninstallable exact68c AUTH-enabled test configuration/driver build as a distinct test artifact, or another already-supported production-compatible gate; completed productionOFF artifact identity remains separate. This helper does not implement either choice. Root was notified immediately of the constraint; graphics decoder-negative coordination confirms actual alternate-source graph proof must use a dedicated private remote rather than assume unrelated sources are visible through portal remote permissions. Priorca299 P1 remains scoped to its explicit no-fallback guard contract, not an observed permissions bypass.

Stopping point: read-only build preparation complete with exact commands/evidence and straightforward incompatibility reported. No compiler/private lease held or requested; board AVAILABLE. Prior exact68c bounded ACCEPT, ca299 REJECT and earlier verdicts unchanged. Compatible help remains same-reviewer repaireddecoder recheck and eventual bounded startup candidate review.
