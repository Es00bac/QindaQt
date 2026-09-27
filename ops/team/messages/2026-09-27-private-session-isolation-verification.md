# Private session isolation verification

2026-09-27T01:14:10.497246+00:00

Branch fix/private-session-host-isolation, base d3b646f2b39665fd65b124c879493b2baf36f656.

Owned changes: src/session_supervisor and tests/session_supervisor policy,
publication and refresh helpers; compositor-session/testing-harness wiki;
ADR-0276 and its navigation entries; own ops work records.

Verification:

- Configure: `cmake -S . -B build/isolation -G Ninja -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release` — exit 0.
- Build: `cmake --build build/isolation --parallel 4 --target qindaqt-session qindaqt_session_activation_policy_tests qindaqt_activation_environment_tests qindaqt_resident_service_refresh_tests` — exit 0.
- Focused `ctest --test-dir build/isolation --output-on-failure -R 'qindaqt.session-(activation-policy|activation-environment|resident-service-refresh)$'` — 3/3 gates, 30 QtTest results including setup/cleanup, exit 0.
- `python3 tools/validate-docs` — 416 documents and navigation, exit 0.
- `mkdocs build --strict --site-dir build/docs-isolation` using the existing user MkDocs venv — exit 0.
- `git diff --check` — exit 0.

Binary: `build/isolation/src/session_supervisor/qindaqt-session`.

No physical or nested compositor was launched and no live user manager was
mutated during qualification. Private harnesses must own their broker setup;
that changed contract is documented. Existing physical private-bus fallback is
covered through the native route policy and explicit fake-manager transport.
Next action: independent exact-candidate review, then manager integration and
atomic staging of the session executable without restarting the physical
session. Available to repair review findings; no other compatible outcome is
claimed while these paths await review.
