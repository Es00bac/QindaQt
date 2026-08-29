# Euler the 2nd File Manager S0 integration-repair handoff

- Time: 2026-08-28T08:27:37-06:00
- Implementer: Euler the 2nd
- Exact repaired candidate:
  `4c2821debb76c3d3c90c5bca61ecd13d5e37411b`
- Tree: `9185cb362c0c33f26c68faa0df3fcb524eeb9bb6`
- Exact parent: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
- Branch/worktree: `worker/file-manager-s0-repair-euler` in
  `/home/cabewse/work_SPaC3/container-wm-workers/file-manager-s0-repair-euler`
- Worktree: clean

## Delivered repair

1. `src/apps/file_manager/CMakeLists.txt` and `main.cpp` remove the production
   absolute build-QML import. The executable discovers the Qt QML root from an
   install-relative path and carries a matching relative Tokens `INSTALL_RPATH`.
2. The `FileManager` install component is now self-contained for the exact
   Tokens/Controls runtime libraries, plugins, metadata, and Qt-generated
   Controls QML deploy inventory.
3. `tests/apps/file_manager/run_installed_file_manager.cmake` adds a clean,
   deletion-guarded component stage. It requires the exact app/desktop/theme/
   Tokens/Controls payload, checks desktop metadata and generated Controls
   inventory, rejects an embedded build QML directory, clears ambient QML and
   dynamic-library paths, checks all five themes, then constructs the real QML
   root offscreen with deterministic `--check-qml-root` exit.
4. File Manager and ADR prose now state that AppShell is integrated but this
   S0 intentionally does not migrate to it. The manager-reserved File Manager
   decision is ADR-0029; filename, title, links, index, and nav all moved from
   the provisional ADR-0028.

Read-only local navigation and bounded `QDesktopServices` launch authority are
unchanged. No AppShell dependency, portal, write operation, host path authority,
or session behavior was added.

## Evidence (exact descendant source tree)

- `python3 tools/check-source-shape` → PASS, 1029 files, zero allowlist skips;
- `python3 tools/validate-docs` → PASS, 65 Markdown pages plus nav;
- `uvx --from mkdocs mkdocs build --strict` → PASS;
- `git diff --check` and `git diff --check HEAD^ HEAD` → PASS;
- staged runner CMake parses through its required-input guard as expected;
- `git status --porcelain=v1` → empty.

## Bounded caveats and requested action

Per manager instruction I did **not** configure, compile, execute the File
Manager, launch a GUI/session, or run the private stage while Victor owns the
serialized lane. Therefore this handoff is source-safe, not compiled/runtime
qualification. A different worker must rereview this exact immutable commit.
If accepted, run Curie's serial target order, all eight `file-manager` CTest
rows including the staged package row, the integrated AppShell/QST/Controls and
Power regressions, broad dependency-light registry, and private offscreen stage
before integration credit. Route a concrete reproduction back to this exact
worktree for a non-amended descendant if any gate fails.
