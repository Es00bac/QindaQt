# Dorian Vale — current-base virtual-desktop review finding

- Timestamp: 2026-08-28T11:52:47Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Exact candidate: `478435ef10024d3747d959f5bb198e60f9277c99`
- Exact tree: `a032cddcac22281d68735c1910501c4121101e12`
- State: **working; blocking P1 reproduced**

## Identity and safe gates

Commit, tree, ordered parents and the exact 23-path first-parent scope match the
handoff. The recomputed sorted path manifest is exactly
`6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.
Fifteen of the accepted 20 virtual-desktop paths are byte-identical to
`f28f443b`; the five shared registry/docs paths are additive combined diffs.
Fresh desktop-session units pass 37/37, Python compilation passes, the exact
Compositor1 descriptor check passes, source shape passes 993 files, docs/nav
passes 64 documents, and the detached worktree/diff are clean. No compile,
resident, bus, compositor, session, display, input or host-runtime action ran.

## P1 — dock evidence does not bind the production shell

`DevelopmentShellSurfaces` deliberately exports each layer surface's client
`processId` (`src/compositor/kwin/kwincontrolendpoint.cpp:263-268`). S1 claims a
mapped and committed **production** shell dock (`docs/wiki/adr/0026-contain-
virtual-desktop-qualification.md:60-66`), and the topology already authenticates
the shell PID. But `_validate_input_and_dock()` accepts any record with the
right scope/output/mapped/committed values and never checks `processId`
(`tests/session/desktop_session_topology.py:261-285`). The positive fixture
omits that field entirely (`tests/session/test_desktop_session_topology_unit.py:
67-75`). Consequently a foreign layer-shell client can satisfy the claimed
production-dock proof.

Exact safe reproduction on the immutable candidate:

```text
PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
import sys
sys.path.insert(0, 'tests/session')
from test_desktop_session_topology_unit import valid_evidence
from desktop_session_topology import validate_boot_evidence
evidence = valid_evidence()
evidence['dockSurfaces'][0]['processId'] = '999999'
validate_boot_evidence(evidence)
print('REPRO: forged dock processId=999999 accepted as production dock')
PY
REPRO: forged dock processId=999999 accepted as production dock
```

Smallest repair: retain and require the exact decimal-string `processId`, bind
it to authenticated `pids["shell"]` during final evidence construction/
validation, and add negative rows for missing, malformed and non-shell PIDs.
Readiness may remain a presence check, but the preserved final artifact must
not validate without this identity binding.

## Remaining bounded review

The endpoint delta itself is limited to adding exact scope `dock` behind the
existing pre-inspection `m_mutationsEnabled` gate while retaining both
notification scopes and unrelated-scope rejection. I am finishing the five
additively merged paths, public-current-base retention, and the compositor
reference/ADR-0020 relationship before issuing one exact terminal verdict.

