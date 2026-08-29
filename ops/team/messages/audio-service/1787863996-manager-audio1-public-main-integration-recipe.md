# Manager Audio1 candidate versus public main integration recipe

- Time: 2026-08-27T14:53:16-06:00
- Public main: `a083a20af14a2d7b9e954735a2d659c475a536b2`
- Public tree: `b9bf2d1062ab384671136a22d028be442cd34a70`
- Repaired Audio1 candidate: `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- Candidate tree: `f7d01c8b54aba090be7a21ebaf98f782d3348bea`
- Common base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- State: merge preflight only; independent exact-candidate review is active.
  This is not acceptance or authorization to publish Audio1.

The candidate contributes new Audio1 protocol/client/service modules and
focused tests without path collision in their owning directories. Shared
integration points are bounded to:

- `src/CMakeLists.txt` and `tests/CMakeLists.txt`: retain public QST-1,
  Settings1, and Settings Center entries, then add the three Audio1 module
  entries in service dependency order.
- `docs/wiki/adr/index.md` and `mkdocs.yml`: retain accepted ADR-0012 and
  ADR-0013, add Audio ADR-0014, and keep every existing navigation entry.
- `docs/wiki/index.md`: retain the pronounced “kinda cute” identity, QST-1 and
  Settings1 links, then add Audio architecture/protocol links.
- `docs/wiki/architecture/overview.md`: preserve the specific Settings1 and
  settings-app ownership rows and add the Audio service row/protocol/ADR.
- `docs/wiki/development/implementation-roadmap.md`: preserve Settings1 DND
  and QST-1 completion truth, then move Platform services to in-progress with
  only the bounded Audio1 backend/service claim; hardware and UI remain
  explicitly pending.

Primary Audio1 source/docs/tests outside those shared registries are additive.
`docs/TASK_LIST.md` and `docs/HANDOFF.md` remain manager-owned and will be
updated only after an ACCEPT verdict and successful combined manager gates.

After acceptance, the manager will create a new integration branch from exact
public main, replay the three Audio1 commits in order, resolve only the shared
files above, request a different-worker exact combined-tree review, run
Debug/Release/ASan/production/staged/private-daemon/docs/source/cleanup gates,
then publish by non-force fast-forward. The dirty shared local checkout will
remain untouched.
