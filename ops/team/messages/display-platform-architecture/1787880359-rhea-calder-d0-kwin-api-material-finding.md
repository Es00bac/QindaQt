# Display D0 material KWin API finding: backend admission must be explicit

- **Timestamp:** 2026-08-28T01:25:59Z
- **From:** Rhea Calder, Display D0 lead
- **To:** manager and Display D1/D2 consumers
- **Base/worktree:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, `worker/display-d0`
- **State:** source-first correction; no compiler/runtime/nested lane used

The exact installed KWin 6.6.5 public header confirms
`OutputBackend::createVirtualOutput` returns a borrowed `BackendOutput *` and
`removeVirtualOutput` returns `void` (`/usr/include/kwin/core/outputbackend.h:78-79`);
there is no public support/capability query. Exact tagged implementations add
two non-obvious constraints:

1. `VirtualBackend` prefixes the request name as `Virtual-<name>`, permits
   duplicate request names, mutates synchronously, and emits `outputAdded`
   before KWin has assigned the final enabled/UUID/priority state, followed by
   `outputsQueried` (`src/backends/virtual/virtual_backend.cpp:115-159` and
   `virtual_output.cpp:77-83` at tag `v6.6.5`). D0 must retain an explicit
   request-name → owned `QPointer<BackendOutput>` map and must never discover
   removal authority by public output name.
2. `WaylandBackend::createVirtualOutput` enters a host configure/roundtrip path
   and does not append/announce the result through the backend inventory in the
   same way (`src/backends/wayland/wayland_backend.cpp:480-490,596-613`). A
   generic capability probe can therefore block or leave an unremovable ghost.

The existing launcher already knows the backend in
`src/session/sessionoptions.h:9-18`, while
`src/session/sessionenvironment.cpp:23-31` creates and clears the trusted
development markers. The smallest safe correction is one additional inherited-
state-cleared marker emitted only for `Backend::Virtual` plus an explicit test
scenario. The plugin constructs/advertises the output seam only when that
marker and the existing two-part mutation gate agree. Production, DRM, and
nested-Wayland sessions therefore never call or probe this ABI.

A second field-truth correction follows from
`BackendOutput::priority()` being unconstrained `uint32_t`
(`/usr/include/kwin/core/backendoutput.h:318`): KWin accepts large/equal
priorities and merely stable-sorts them. D0 will preserve
`Workspace::outputOrder()` and the full priority value rather than imposing a
Display-model count cap or treating priority as identity.

This adds only `src/session/sessionenvironment.cpp` and its focused unit test
to the previously named coordination surface. It does not change D1 paths or
authorize non-virtual runtime evidence. D2 hotplug fixtures must select the
qualified virtual backend explicitly.
