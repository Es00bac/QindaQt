# File Manager S0 integration preflight midpoint

- Time: 2026-08-28T14:09:15Z
- Owner: Curie the 2nd
- Exact pair: public `cbec6fb42216e5bcc3283004473be7f5f6ccda66`
  and candidate `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`

## Evidence and findings

`git merge-base` returned the candidate parent
`9db68c4023257b49421101fa1b13c73bbc2cfa85`. Exact
`git merge-tree --write-tree --messages` returned exit 1 and only these three
content conflicts:

- `docs/wiki/adr/index.md`: retain both ordered ADR-0027 and ADR-0028 rows and
  remove the candidate-base note that calls integrated ADR-0027 in flight.
- `docs/wiki/index.md`: retain both AppShell and File Manager entries.
- `mkdocs.yml`: retain both ordered ADR-0027 and ADR-0028 navigation entries;
  the Applications section already auto-unions AppShell, Text Editor, and File
  Manager.

`src/CMakeLists.txt`, `tests/CMakeLists.txt`, and
`docs/wiki/architecture/module-boundaries.md` auto-merge as additive unions.
The generated merge tree keeps AppShell, Power/Brightness, and File Manager;
there is no source-path collision.

Two non-mechanical risks remain. Candidate documentation at
`docs/wiki/apps/file-manager.md:15-18,166-168` and
`docs/wiki/adr/0028-file-manager-bounded-local-launch.md:39-41,61-73` says
AppShell is unreviewed/unintegrated, which is false on public `cbec6fb`; the
integration resolution must say AppShell is integrated but File Manager S0
deliberately has not migrated to it. Also
`src/apps/file_manager/CMakeLists.txt:47-60` directly links the installed
executable to shared `QindaQt::TokensQml`, whose library installs below the Qt
QML tree, but the executable defines no install RPATH and the candidate has no
staged-installed runtime test. QML engine import paths do not prove the ELF
loader can start the executable; this needs an executable package gate and may
require a repair before integration.

Current problem: source/static evidence cannot decide either configure-time
QML import resolution or installed loader behavior. Help requested: none; the
manager should reserve a serial compile/package lane only after correctness
review. Next action: enumerate exact drift and hand off the deterministic gate
order without approving the candidate.
