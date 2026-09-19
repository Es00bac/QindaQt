# PASS — bounded Viewer build/CI guard review

- Manager baseline: `fce298f10f06845a4740dfd6cbd64ef4a1fd20a6` plus its four-file unstaged build/CI patch.
- Patch SHA-256: `cee3d48cf029896b01a2999ffb86f43e6883ff5b8b88388894326b0c90c313c0`.
- Files: `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/ci.yml`.
- Findings: **none** in this bounded review.

The copied files were verified byte-identical to the manager's files before
configuration. Viewer defaults to ON, both source and focused test registration
are conditional, and all three hosted CI jobs explicitly opt out. README,
ADR0218, release procedure and testing harness correctly distinguish reduced
hosted-CI coverage from required enabled native desktop/package builds. Enabled
Viewer retains required QindaTK and Poppler discovery in its owning module.

Independent executable evidence in the isolated reviewer worktree:

```sh
cmake -S . -B .cache/build-review-viewer-off -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_BUILD_VIEWER=OFF \
  -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF \
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DCMAKE_DISABLE_FIND_PACKAGE_QindaTK=ON
```

Configure/generate exit **0**. The deliberately disabled QindaTK find variable
is reported unused, confirming its discovery path was never visited. A cache
and generated graph probe exits **0**: no `QindaTK_DIR`, no Poppler frontend
lookup cache, no viewer source/test binary directories, and no viewer targets.
`ctest --test-dir .cache/build-review-viewer-off -N -R '^apps.viewer\.'`
exits **0**, **Total Tests: 0**.

The exact copied patch was then reversed only in the isolated reviewer
worktree. Its source is again clean at accepted
`b724e5667d481b79227fd7fa22ae48104c014ecb`; root product files were never edited.
Logs and copied patch remain under ignored `.cache/build-review-viewer-off`
and `.cache/review-extra/manager-build-guards.patch`.

Requested next action: manager finishes the separate integrated ON build and
combined gates. No further repair is requested from the viewer implementer.
Bounded help remains available for an exact integration regression or installed
viewer failure; no physical-display or remote-build coverage is inferred here.
