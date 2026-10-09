# File Manager and catalog Windows executable-open candidate

2026-10-09T00:55:16.122818+00:00

Exact candidate `4e62256b961e87d2f8b6e63f8e822b85f65ff6e3`, exact integrated base `c265e4e661025bd400f7d0ed8c075d420b8064af`, own `worker/everyday-windows-catalog-sol-20261009`. The source consumes public typed Windows bindings, inspects actual local DOS/PE/COFF/optional/section headers, and retains descriptor/dev/inode/size/mtime/ctime through setup. Both double-click and Open With share a public profile/setup route, preserve chosen native defaults and same-ID native desktop overrides through exact retained-content digest, distinguish Starting/SetupOpened/OutcomeUnknown and never fall back after Windows submission. Refused is zero operational side effect; a concrete adapter must transactionally hold registration/profile authority at dispatch. ADR0370 separates user-file receipts from Portage guest recipe provenance.

Changed paths:

- `docs/wiki/adr/0370-admit-user-selected-windows-programs-through-leased-pe-files.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/file-manager.md`
- `docs/wiki/architecture/foreign-applications.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/windows-runner.md`
- `mkdocs.yml`
- `src/apps/file_manager/CMakeLists.txt`
- `src/apps/file_manager/model/launch_intent.cpp`
- `src/apps/file_manager/model/launch_intent.h`
- `src/apps/file_manager/model/open_with_launcher.cpp`
- `src/apps/file_manager/model/open_with_launcher.h`
- `src/apps/file_manager/public/desktop_file_boundary.cpp`
- `src/apps/file_manager/public/desktop_file_boundary.h`
- `src/windows_runner/CMakeLists.txt`
- `src/windows_runner/include/qindaqt/windows_runner/catalog_binding.h`
- `src/windows_runner/include/qindaqt/windows_runner/executable_file.h`
- `src/windows_runner/include/qindaqt/windows_runner/executable_open.h`
- `src/windows_runner/include/qindaqt/windows_runner/launch_ports.h`
- `src/windows_runner/src/catalog_binding.cpp`
- `src/windows_runner/src/executable_file.cpp`
- `src/windows_runner/src/executable_open.cpp`
- `tests/windows_runner/CMakeLists.txt`
- `tests/windows_runner/pe_fixture.h`
- `tests/windows_runner/tst_catalog_binding.cpp`
- `tests/windows_runner/tst_executable_file.cpp`
- `tests/windows_runner/tst_file_manager_windows_open.cpp`

Actual evidence: `cmake -S tests/windows_runner -B .cache/windows-exe-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`0/.665s; ONE `cmake --build .cache/windows-exe-build -j2`0/53.110s; `ctest --test-dir .cache/windows-exe-build --output-on-failure`5/5/.06375s. Direct LastTest has79Qt passed (28planning,16coordinator,7catalog,20PE,8FileManager) with0failed/skip/blacklist. `mkdocs build --strict --site-dir <ignored-site>`0/8.476s and `python3 tools/validate-docs`0/.464s,541documents/navigation. Raw `.cache/windows-exe-focused-20261009T005114Z`, `.cache/windows-exe-build/Testing/Temporary/LastTest.log`. Actual2CPU2GiB/swap0/tasks128/CPU4,16/nice10; unit62.88s,CPU113.52s,peak1.2GiB. Initiator2964946 reaped; unitnot-found/inactive/dead/MainPID0; small slot released. No Wine, live input, host default or package execution.

Bounded caveats: the source smoke uses never-executed header-format fixtures and an injected Windows profile/setup port. Default composition still supplies no concrete Windows port. Full FileManager/SDK target was not rebuilt; the focused leaf compiles the two changed route cpp files, while the small public FileBoundary wrapper is reviewed at source. Concrete receipt/profile persistence, actual visible setup, stable-handle process adapter and compiled Portage desktop/MIME handler remain required. Do not label valid user `.exe` launch, ordinary Windows app journey, blue window authority, Audio1 or HMENU Global Menu complete.

Requested next action: independent exact source/public-ABI review and focused integration, then concrete first-run/profile/handler source from accepted integrated base. Preserve the no-long-native-loop instruction and explicit user-file provenance.
