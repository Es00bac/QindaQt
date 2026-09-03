# Phyllis Fox handoff — File Manager S1 local mutation and Trash

- Candidate commit: `61283bf017990694a9ddc3f183f54751c3ddf849`
- Candidate tree: `242ff7ca98176af000f2a624aea6a8ea6e641346`
- Exact base: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Branch: `worker/file-manager-s1`
- Requested next action: independent exact review of candidate
  `61283bf017990694a9ddc3f183f54751c3ddf849`, then Program Manager integration.

## Outcome

The candidate adds one typed asynchronous local-mutation boundary for new
folder, rename, bounded copy, same-filesystem move, home Trash, restore, and
empty Trash. Descriptor-relative path operations pin parents, refuse symlinks,
recheck source and destination identities, and clean partial copies without
following replacement links. The home Trash uses exclusive, flushed,
freedesktop `.trashinfo` metadata and truthfully refuses cross-device moves.
AppShell actions provide keyboard/context dispatch, destructive confirmations,
accessible bounded state, cancellation, and one-level undo/restore.

## Changed paths

- `docs/wiki/adr/0064-confine-file-mutation-to-identity-checked-local-authority.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/file-manager.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `src/apps/file_manager/CMakeLists.txt`
- `src/apps/file_manager/app_shell/file_manager_action_catalog.cpp`
- `src/apps/file_manager/app_shell/file_manager_action_catalog.h`
- `src/apps/file_manager/main.cpp`
- `src/apps/file_manager/model/file_manager_types.h`
- `src/apps/file_manager/model/local_directory_lister.cpp`
- `src/apps/file_manager/model/navigation_controller.cpp`
- `src/apps/file_manager/mutation/device_resolver.cpp`
- `src/apps/file_manager/mutation/device_resolver.h`
- `src/apps/file_manager/mutation/home_trash.cpp`
- `src/apps/file_manager/mutation/home_trash.h`
- `src/apps/file_manager/mutation/local_mutation_backend.cpp`
- `src/apps/file_manager/mutation/local_mutation_backend.h`
- `src/apps/file_manager/mutation/mutation_backend.h`
- `src/apps/file_manager/mutation/mutation_controller.cpp`
- `src/apps/file_manager/mutation/mutation_controller.h`
- `src/apps/file_manager/mutation/mutation_types.cpp`
- `src/apps/file_manager/mutation/mutation_types.h`
- `src/apps/file_manager/mutation/safe_path_operations.cpp`
- `src/apps/file_manager/mutation/safe_path_operations.h`
- `src/apps/file_manager/mutation/safe_tree_operations.cpp`
- `src/apps/file_manager/mutation/safe_tree_operations.h`
- `src/apps/file_manager/ui/EntryList.qml`
- `src/apps/file_manager/ui/Main.qml`
- `src/apps/file_manager/ui/MutationDialogs.qml`
- `src/apps/file_manager/ui/Toolbar.qml`
- `tests/apps/file_manager/CMakeLists.txt`
- `tests/apps/file_manager/check_mutation_boundary.cmake`
- `tests/apps/file_manager/tst_file_manager_action_catalog.cpp`
- `tests/apps/file_manager/tst_home_trash.cpp`
- `tests/apps/file_manager/tst_local_directory_lister.cpp`
- `tests/apps/file_manager/tst_local_mutation_backend.cpp`
- `tests/apps/file_manager/tst_mutation_controller.cpp`

## Acceptance evidence

All commands ran from the candidate worktree. Build output stayed below
`/home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1`.

- `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`: exit 0.
- `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`: exit 0.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/debug --parallel 3 --target qindaqt-file-manager qindaqt_file_manager_history_tests qindaqt_file_manager_local_mutation_tests qindaqt_file_manager_home_trash_tests qindaqt_file_manager_mutation_controller_tests qindaqt_file_manager_action_catalog_tests qindaqt_file_manager_local_lister_tests qindaqt_file_manager_launch_intent_tests qindaqt_file_manager_controller_tests`: exit 0.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/release --parallel 3 --target qindaqt-file-manager qindaqt_file_manager_history_tests qindaqt_file_manager_local_mutation_tests qindaqt_file_manager_home_trash_tests qindaqt_file_manager_mutation_controller_tests qindaqt_file_manager_action_catalog_tests qindaqt_file_manager_local_lister_tests qindaqt_file_manager_launch_intent_tests qindaqt_file_manager_controller_tests`: exit 0.
- `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/debug -R '^qindaqt\.file-manager-' --output-on-failure --no-tests=error`: exit 0, 14/14 passed.
- `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/release -R '^qindaqt\.file-manager-' --output-on-failure --no-tests=error`: exit 0, 14/14 passed.
- During descriptor hardening, the Debug test command above with selector `'^qindaqt\.file-manager-(local-mutation|home-trash)$'`: exit 0, 2/2 passed.
- `./tools/validate-docs`: exit 0, 128 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/file-manager-s1/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2,065 files checked; only pre-existing out-of-lane review-threshold warnings were reported.
- `git diff --check`: exit 0.
- `git diff --cached --check`: exit 0 before the immutable product commit.

One exploratory focused build named two nonexistent shorthand targets and
exited 1 before compilation; target discovery identified the registered
`qindaqt_file_manager_*` names, and every corrected/final build above passed.
No host display, session/system bus, hardware, mount, network, uinput, or nested
compositor row was used.

## Bounded caveats

This candidate deliberately does not claim mounts, per-volume Trash, permanent
delete outside confirmed Empty Trash, search, previews/thumbnails, portals,
network locations, durable multi-operation recovery, nested screenshots, or
whole-application assistive-technology qualification. Restore is the latest
successful in-process Trash operation; copy intentionally has no automatic
undo. Filesystem metadata preservation remains best-effort where the platform
permits it.
