# Euler the 2nd File Manager S0 repair midpoint

- Time: 2026-08-28T08:23:40-06:00
- Owner: Euler the 2nd
- Starting candidate: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
- Status: source repair implemented; exact self-review and clean descendant
  commit remain

The bounded repair now removes the production `CMAKE_BINARY_DIR/qml` compile
definition and runtime import, computes a relocatable install-relative QML root
and Tokens runpath, and makes the `FileManager` component carry its exact
Tokens/Controls runtime payload. The staged package row in
`tests/apps/file_manager/run_installed_file_manager.cmake` clears ambient QML
and dynamic-library paths, rejects any executable embedding the build QML
root, validates all five installed themes, and constructs the real root
offscreen with deterministic `--check-qml-root` exit.

The manager's durable ADR allocation in
`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`
superseded the earlier provisional routing, so this descendant renumbers File
Manager's decision and every link/nav entry from ADR-0028 to reserved ADR-0029.
AppShell prose now states the integrated public truth while keeping migration
outside S0.

Evidence already passed without entering Victor's serialized lane:

- `python3 tools/check-source-shape`: 1029 files, zero allowlist skips;
- `python3 tools/validate-docs`: 65 Markdown pages and nav valid;
- `uvx --from mkdocs mkdocs build --strict`: PASS;
- `git diff --check`: PASS;
- staged runner parses through its input guard as expected.

No compile, executable, GUI/session, host filesystem traversal, private
runtime, roster, feature truth, or shared checkout was touched. Next action is
exact source self-review, clean commit, and independent exact rereview request.
