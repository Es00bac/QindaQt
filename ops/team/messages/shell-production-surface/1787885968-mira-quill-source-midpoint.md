# Mira Quill midpoint: deterministic surface proof is source-complete

- **Timestamp:** 2026-08-28T02:59:28Z
- **Status:** source/static midpoint complete; nested qualification pending
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch:** `worker/shell-surface-repair`

The narrow repair is authored without changing production shell policy or any
Controls path:

- `tests/session/fixtures/shell_surface_profiles/qindaqt-surface-proof.json:1`
  is a schema-v1 two-panel profile retaining the qualified 30-pixel top bar and
  centered 52%-width, 54-pixel shelf while making both hide modes `never`.
- `tests/session/CMakeLists.txt:165` selects that fixture and passes its exact
  identity for every 1080p/WUXGA/1440p row. An `AGENT-GUARD` prevents the
  historical maximized-client/intelligent-hide race from being reintroduced.
- `tests/session/test_shell_surface_nested.py:68` rejects a wrong file/id,
  panel set, or hide mode before KWin starts, then publishes the accepted ID
  through the isolated environment at lines 198-200.
- `tests/session/shellsurfaceprobe.cpp:234` requires the explicit ID and sends
  it to the unchanged production `qindaqt-shell` command instead of hard-coding
  the user-facing `qindaqt` profile.
- `docs/wiki/development/testing-harness.md:198` and
  `docs/wiki/shell/panel-surfaces.md:127` now distinguish this deterministic
  initial-publication/work-area proof from the still-separate live
  automatic-hide protocol-transition boundary.

Non-compiler static evidence is clean:

- `git diff --check`: exit 0.
- `jq` fixture identity/count/hide-mode assertion: true, exit 0.
- Python bytecode compile for the nested runner and protocol validator: exit 0.
- direct `validate_proof_profile(...)` invocation: exit 0.
- `python3 tools/docs_validation.py`: 47 Markdown documents/navigation valid.
- `tools/check-source-shape`: 831 files checked, zero allowlisted skips.

`mkdocs build --strict` is presently unavailable in this worktree environment
because `mkdocs` is not installed (`command not found`); the repository's
stdlib link/navigation validator passed. No configure, compiler, CTest, nested
KWin, host desktop, input, session, display, or physical-output action ran.

The worktree remains uncommitted. It now waits for explicit compiler transfer
before the exact three private nested rows and focused shell regressions.
