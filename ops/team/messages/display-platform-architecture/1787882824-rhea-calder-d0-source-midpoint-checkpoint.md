# D0 source/static midpoint checkpoint

- **Timestamp:** 2026-08-28T02:07:04Z
- **Worker:** Rhea Calder — Display D0 compositor-output lead
- **HEAD/base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch/worktree:** `worker/display-d0` at `/home/cabewse/work_SPaC3/container-wm-workers/display-d0`
- **State:** waiting/not live; source/static integration is coherent, compiler/runtime lane remains unassigned

## Integrated source outcome

- The retained output inventory boundary is complete: `src/compositor/kwin/kwinoutputinventory.h:20-88` defines the full ordered value projection, shared shell bounds, atomic retained store, and monotonic generation; `:90-103` defines the borrowed-KWin/value-copy lifetime boundary. `src/compositor/kwin/kwinoutputinventory.cpp:129-224` validates and serializes one canonical generation, while `:354-382` samples KWin's semantic output order and exact integral visibility geometry.
- The shell visibility path now consumes that same retained generation rather than resampling raw output state. The public schema carries a mandatory canonical decimal `outputGeneration`; metadata-only and integral-shell-boundary-only changes advance the shared generation without duplicate signal publication.
- The development virtual-output seam is complete and independently bounded. `src/compositor/kwin/kwindevelopmentoutputseam.cpp:78-143` rejects before parse/backend access when disabled, validates logical dimensions/scale/name, and removes only owned outputs. Its exact KWin adapter preserves request-name to `QPointer<BackendOutput>` identity, raw/prefixed collision checks, synchronous ownership, no-op retention, and clear-before-teardown.
- `src/compositor/kwin/qindaqtkwinplugin.cpp:38-69` constructs and advertises the seam only under the launcher-proven exact VirtualBackend marker; the broad development mutation flag cannot enable it. Endpoint capabilities omit the feature otherwise, and plugin teardown unpublishes D-Bus before synchronously removing owned test outputs.

## Tests and documentation integrated

- Pure inventory tests cover stable generation, metadata and shell-boundary changes, duplicate identity rejection, full unsigned priority/equal ordering, shared count/scale limits, retained prior state, and exhaustion.
- Controller and pinned-KWin fake-backend tests cover pre-parse disabled behavior, validation/limits, raw plus `Virtual-` collision, exact-pointer synchronous removal, no-op authority retention, preservation of external outputs, and owned-only teardown.
- `tests/session/compositoroutputworkflow.cpp:104-158` specifies add/remove hotplug with exact `+1/+2` generations, one `OutputsChanged` hint per accepted transition, and `Outputs`/`ShellVisibilitySnapshot` generation agreement. `:161-184` proves hostile and valid production calls have the identical disabled reply with unchanged inventory and signal count.
- Protocol 1.1, inventory schema/bounds/generation, exact three-fact development gate, logical-size semantics, ownership/teardown, module boundaries, session behavior, and the nested qualification matrix are documented. The normative seam contract is at `docs/wiki/reference/compositor-control-v1.md:74-92,111-160,224-249`; the nested hotplug qualification row is at `docs/wiki/development/testing-harness.md:447-459`.

## Static evidence and diff check

Against exact HEAD, the preserved working tree contains 42 modified tracked paths (`573` additions, `184` deletions) and 8 new D0-owned paths (1,533 lines total). No foreign or unexplained write was found.

The following source/static commands all exit 0:

```text
git diff --check
python3 tests/compositor/test_dbus_contract.py compositor/dbus/org.qindaqt.Compositor1.xml compositor/dbus/service.json
python3 -m py_compile tests/compositor/test_dbus_contract.py tests/session/test_nested_session.py
tools/check-source-shape
```

`tools/check-source-shape` checked 839 source files with 0 allowlisted skips. The only initial findings were two Python validators at 125 and 123 lines; I extracted cohesive signature/output validators and the re-run is clean. All new hand-written production files remain under the 500-line decomposition-review threshold (`kwinoutputinventory.cpp` 397 lines; `kwindevelopmentoutputseam.cpp` 285 lines).

## Material unresolved issue / checkpoint decision

There is no known remaining source-shape or contract-drift issue. The material unresolved gate is compiler qualification of the exact KWin 6.6.5 fake subclasses, Qt automoc/D-Bus signal wiring, focused unit targets, then nested VirtualBackend behavior and strict docs/link gates. I ran none of those commands and make no runtime claim because Controls owns the sole serial lane and repaired D1 is next.

The source-only D0 outcome is ready as a preserved compiler-blocked checkpoint. I have not committed it; the worktree remains intact for resume when the manager assigns the lane.
