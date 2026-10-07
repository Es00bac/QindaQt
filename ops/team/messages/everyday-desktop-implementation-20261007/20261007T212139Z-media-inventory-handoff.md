# ED-04 readonly media inventory/client handoff

- Time: 2026-10-07T21:21:39+00:00
- Worker: ed-media-delivery-sol-20261007
- Exact verified runtime commit: 32617cf42f707bf682cbc7979e8cc7e0cd46bbfb
- Exact base: 2188d8e0e339ce4b56acb841a4b570f58a3002cc
- Branch/worktree: worker/everyday-media-delivery-20261007 / everyday-media-delivery-20261007
- Outcome: read-only Devices exporter + installed public exact-owner client; not ED-04 consumer completion.
- Source paths: src/services/removable_media_client; src/apps/removable_media/media_exporter*, media_public_projection*, private Volume roots/partition/read-only facts/authorityGeneration, CMake/main; src/CMakeLists; tests/services/removable_media_client and tests/CMakeLists; media/client/module/protocol docs and mkdocs.
- Source gate: cmake -S tests/services/removable_media_client/standalone -B .cache/media-inventory-build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER_LAUNCHER=ccache; cmake --build .cache/media-inventory-build -- -j24 -l24. Exit0 with -Wall -Wextra -Wpedantic -Werror.
- Tests: ctest --test-dir .cache/media-inventory-build --output-on-failure. Exit0, 4/4 CTest, policy10/UDisks10/notifications6/inventory8 Qt checks, 34 total zero failures/skips.
- SDK: cmake --install .cache/media-inventory-build --prefix ignored-stage/usr; isolated consumer links only staged include and client/protocol archives plus Qt Core/DBus. Both buses absent:1/1 CTest exit0, zero launch. Staged client header withheld: clean compilation failed exactly No such file, then restored build/1/1 pass. Committed installed_consumer source is byte-identical to that executed ignored harness.
- Docs: mkdocs build --strict --site-dir .cache/media-inventory-docs exit0; python3 tools/validate-docs exit0,520 pages; git diff --check exit0.
- Failures repaired: ambiguous Qt fixture reply, missing fixture kind/name and cleanup cascade, initially disconnected-session pending-call silence. Preserve ignored logs in .cache/media-inventory-*.
- Runtime lease: RELEASED compiler/private session buses to Platform after gates. No system-bus/real helper/device/preferences touched.
- Bounded remaining gates: independent exact review; owner QML/native full build; public action admission/readback/no-replay; File Manager and chooser presenters; installed/disposable physical USB journey. No physical/hardware evidence claimed.
- Requested action: root route exact readonly candidate to independent review and integration; worker continues ordinary-action source under no compiler lease.
