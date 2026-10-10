# REJECT — 59136754f0b33f5f672cdd5b2f6f7cb2251a19f7

- Candidate: 59136754f0b33f5f672cdd5b2f6f7cb2251a19f7
- Parent recipe: 10c67e862af129f53b382fff4fac1bf7763332bc
- Deployment source: 42ec5bfa87721dbba05157fdf6030071e71da5d6
- Verdict: REJECT for one bounded staging failure; public-key repair and immutable baseline checks pass.

The R29 recipe builds only `qindaqt-shell`, then runs `cmake --install "${BUILD_DIR}" --component QindaQt` over the entire configured tree. The exact source includes `KDEInstallDirs6`; its `KDEInstallDirsCommon.cmake:416` sets the default install component to the project name, `QindaQt`. Untagged installs therefore join this component, including `qindaqt-wm` at `src/session/CMakeLists.txt:47`, which the shell target does not build. A fresh shell-only build will fail the stage before `doexe`.

Direct bounded reproduction against the owner's existing focused build:

```
DESTDIR=<review-worktree>/.cache/rejected-stage-repro cmake \
  -DCMAKE_INSTALL_COMPONENT=QindaQt -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LOCAL_ONLY=TRUE \
  -P <owner-build>/src/session/cmake_install.cmake
```

Exit 1: `file INSTALL cannot find .../src/session/qindaqt-wm`; the binary is absent. This runs no compiler and writes no live root. The generated script exposes the same default-component install rule as the pinned source.

Requested repair: stage through the shell's generated install script with the existing prefix/component and `CMAKE_INSTALL_LOCAL_ONLY=TRUE`, or an equivalently narrow shell install rule that preserves CMake's installed RPATH processing. Keep the final `doexe` replacement limited to `usr/bin/qindaqt-shell`. Re-review only this repair and the same reproduction; no repeated build/baseline audit is needed.

Passed evidence, exit 0:

- Deployment source differs from installed R23 only in the seven accepted product paths. Gather production/controller/test files match accepted `2d99ca7a0` exactly.
- Both qinda task-scoped distfiles match their added Manifest sizes and BLAKE2B/SHA512 hashes. R28 archive is 62,392,320 bytes, SHA256 `cc5fd4e7ea7a4e6bca40eeb0da7b12767997728509a860c32be6a8b793dc436f`; source archive is 39,773,054 bytes, SHA256 `d823562ca68a11d975f9ae085d8edf9a721a055345c7c66f1c286d3e14fd2831`.
- Ledger SHA256 `cba2ab6086c1b48dfb2b2c9e1fc2bdf4673ce3da8a537c0eb10c4ad2215c9b3e` matches all 2,622 known R28 image entries exactly: path/type/mode/symlink targets/regular-file bytes and hashes.
- Imported only public certificate SHA256 `bb6f433c1643aab09bd9fff0d74602ac7b2fa1cbcb6cadb990d41fc425fd134f` into a temporary public verification home. Its primary certificate fingerprint is `D17754527D48BA5C24E3CC57179DF9EBE9792BCB`, with expected signing subkey `D1E92C77D6790D8B672663661E6D6993047EE35F`; no private key packets are present.
- Portage gpkg full signature verification passes with signature requirements enabled, including image verification through `_verify_binpkg()` rather than metadata-only validation. Embedded R28 ebuild hash equals `470c8a246e7f3c9c3a197fa28e1659dd44fb5c760ca56caa08f7ddfb77942f46`; PF/source match the retained image. Host keyring/trust configuration is unchanged.
- Runtime dependency declarations retain the installed R28 contract; README identifies pending artifact/native/adoption gates, and shared delivery metadata is unchanged.

No artifact or installed fix is claimed. Native old-shell evidence remains provisional while root checks mapped overlay and input geometry. Only the exact stage defect blocks this recipe.
