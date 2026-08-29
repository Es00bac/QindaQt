# Noether the 2nd — dock-output consistency repair handoff

- **Timestamp:** 2026-08-28T13:57:17Z
- **State:** handoff; no process or work live
- **Candidate:** `58f08ba8499b434e36b2746eff773bd29b2e6c45`
- **Tree:** `ae540d84b2f57b767f8f4ace75234f58a626e44c`
- **Sole parent:** accepted source candidate
  `a1d8c6153f2398f057047331e505850f71143d08`
- **Preserved grandparent:**
  `4e7f6d8448fe1c9cab5ebf3b4605cacaddee008b`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`

## Exact outcome

Every mapping with the production `scope=dock` is now consumed fail-closed:
its `outputName` and `desiredOutputName` must both equal the canonical identity
derived from the one-item public output inventories. Contradictory records are
rejected before mapped/committed cardinality is evaluated, so one valid surface
cannot mask a phantom current or desired output record.

The narrowly permitted Iris P3-1 repair also landed. Exact geometry values must
be non-boolean integers, and scale must be a non-boolean number with the exact
expected value. Focused hostile cases cover `False` origins and `True` scale in
both Outputs and ShellVisibility inventories.

No documentation changed: ADR-0026 and the S0+S1 testing-harness table already
state the exact stronger dock/output and `1920x1080@1` contracts, so prose churn
would not alter current truth.

## Exact manifest

Three paths, sorted path-manifest SHA-256
`fcdfa0abebdc27e14c53178486f182e32cbbd5b674c4b55ff38aed6bff88637f`:

- `tests/session/desktop_session_output.py`
- `tests/session/desktop_session_topology.py`
- `tests/session/test_desktop_session_output_unit.py`

## Post-commit source-safe evidence

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p 'test_desktop_session_*_unit.py'`
  — exit 0, **61/61**.
- In-memory `compile()` over `desktop_session_*.py` plus
  `test_desktop_session_*_unit.py` — exit 0, **14 sources**.
- `tools/check-source-shape` — exit 0, **998 checked**, zero allowlisted skips,
  no warning or issue.
- `tools/validate-docs` — exit 0, **64 Markdown documents** and navigation.
- `git diff HEAD^..HEAD --check` — exit 0.
- `git status --porcelain=v1` — empty; branch is clean at the exact candidate.
- Object-store identity confirms the stated SHA, tree, sole parent, and
  preserved grandparent.

Victor retained the serialized compiler/private-runtime lane throughout. No
configure, build, CTest, nested/private session, bus, compositor, UI, display,
input, cursor, host session/configuration, or hardware action ran.

## Requested next action

Iris Hale or another independent reviewer should rereview exactly
`58f08ba8499b434e36b2746eff773bd29b2e6c45`, bounded to the consumed-dock
current/desired identity rejection, hostile one-valid-plus-one-phantom cases,
and boolean geometry/scale guards. Manager alone integrates and allocates any
later compiler/private-runtime row.
