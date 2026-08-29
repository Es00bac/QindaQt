# Dorian Vale handoff: exact KWin 6.6.5 D0 API counterexample audit

- **Timestamp:** 2026-08-28T01:32:17Z
- **From:** Dorian Vale (`/root/display_d0/kwin_api_audit`)
- **To:** Rhea Calder (`/root/display_d0`), manager/router
- **Exact product base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Mode:** read-only product audit; no product/CMake/Git edit, configure,
  compile, test, compositor, host-session/input/display/config, or KWin-store
  access
- **Upstream pin:** primary KDE KWin tag `v6.6.5`, annotated tag object
  `1b035282ff05101a3441113648a93f57fe0351c1`, peeled commit
  `b04d59c03749484a8a0ed5a8d4cda515a267c59b`

## Result

The bounded seam is valid only as a launcher-admitted **VirtualBackend** seam,
never as a generic `OutputBackend` capability. The value-projected inventory
design is sound when it samples `Workspace::outputOrder()`, coalesces signals,
and advances generation only for a changed complete projection. One blocking
gate bug and one terminology/contract correction remained in the live D0
source snapshot at this handoff.

Installed evidence is exact, not inferred: `kwin-x11 6.6.5-2` and
`KWinConfigVersion.cmake:10` report 6.6.5; installed `main.h`,
`core/outputbackend.h`, `core/backendoutput.h`, and `core/output.h` are
SHA-256 byte-identical to the tagged source. `nm -D /usr/lib/libkwin.so`
exports `OutputBackend::{createVirtualOutput,removeVirtualOutput}` and the
inventory accessors audited below.

## Counterexamples and required boundary

1. **No public capability/result exists.** Installed/tagged
   `src/core/outputbackend.h:66-79` exposes only `outputs()`, a pointer-returning
   create, and a `void` remove. The base implementation returns null on create
   and asserts on non-null remove (`src/core/outputbackend.cpp:80-88`). Concrete
   backend headers are not installed, so a plugin must not cast to a private
   VirtualBackend type.

2. **A generic probe is unsafe.** VirtualBackend prefixes the connector name
   with `Virtual-`, appends synchronously, emits `outputAdded` then
   `outputsQueried`, and on removal emits `outputRemoved`, `outputsQueried`,
   then unrefs (`src/backends/virtual/virtual_backend.cpp:115-159`). By
   contrast WaylandBackend waits in an unbounded host `wl_display_roundtrip`
   loop (`src/backends/wayland/wayland_backend.cpp:480-490`), and its override
   returns an output without appending it to `m_outputs`; its remove can only
   remove members of that list (`:596-613`). A probe can therefore block and
   leave an untracked host window.

3. **Marker/admission rule.** The launcher-cleared
   `QINDAQT_DEVELOPMENT_OUTPUT_BACKEND=virtual` marker is valid as construction
   metadata, not an authentication/security claim. Output mutation requires
   all three exact facts: `QINDAQT_DEVELOPMENT_CONTROL == "1"`, non-empty
   `QINDAQT_TEST_SCENARIO`, and backend marker exactly `virtual`. Do not even
   construct the KWin seam otherwise. `Capabilities.methods` and the
   `developmentOutput` object must then be omitted; the statically introspected
   slots may remain but both valid and malformed calls must return the same
   pre-parse `control-disabled` without consulting KWin.

4. **Blocking live-source counterexample at handoff.** Launcher clearing and
   exact marker creation are correct in
   `src/session/sessionenvironment.cpp:23-37`, and seam construction is exact-
   marker gated in `src/compositor/kwin/qindaqtkwinplugin.cpp:53-60`.
   However, plugin construction at `qindaqtkwinplugin.cpp:63-67` passes the
   broader `m_mutationsEnabled` to `KWinControlEndpoint`, whose
   `kwincontrolendpoint.cpp:56-58` gives that flag to
   `DevelopmentOutputController`. In a dev nested Wayland/DRM session the
   mutator is null but the output controller is enabled: malformed add parses
   to `malformed-virtual-output-request` while valid add reports
   `virtual-output-unavailable`. Pass a distinct exact output-gate boolean to
   this controller. The current Capabilities omission at
   `kwincontrolendpoint.cpp:121-127` is otherwise correct.

5. **Size semantics are backend-dependent.** On the admitted VirtualBackend,
   `createVirtualOutput(size,scale)` stores `size` as logical geometry and
   initializes the pixel mode as `size * scale`
   (`virtual_backend.cpp:115-121,136-139`); `LogicalOutput::geometryF()` divides
   pixel size by scale (`src/core/output.cpp:395-407`). DRM instead makes
   `size` the mode size (`drm_virtual_output.cpp:23-43`). Therefore the D0
   VirtualBackend-only request must call width/height and its bound **logical**
   dimensions, not `pixelSize`/`maximumPixelDimension`. Example:
   1920x1080@1.5 yields a 2880x1620 mode and 1920x1080 logical geometry.

6. **Ownership/removal.** VirtualBackend permits duplicates and supplies no
   idempotence result. Authority must be the plugin's request-name ->
   `QPointer<BackendOutput>` map, never output-name rediscovery. Precheck both
   the requested spelling and `Virtual-<request>` to catch an initial
   `Virtual-0` collision. The live seam does this at
   `kwindevelopmentoutputseam.cpp:208-251`. BackendOutput uses manual ref/unref
   and deletes at zero (`src/core/backendoutput.cpp:193-205`); Workspace adds a
   ref for its LogicalOutput and unrefs removed logical/backend outputs after
   signals (`src/workspace.cpp:1370-1444`). Thus a local QPointer may become null
   before `removeVirtualOutput()` returns. Clear the ownership map before a
   teardown loop, never dereference after remove, retain authority on a backend
   no-op, and make repeated external removal an owned-map error. The live
   `removeVirtualOutput/removeOwnedOutputs` shape at
   `kwindevelopmentoutputseam.cpp:255-285` satisfies this.

7. **Teardown order.** `Application::outputBackend()` is a borrowed pointer to
   the application's unique_ptr (`/usr/include/kwin/main.h:198-202,346-347`),
   set once (`src/main.cpp:510-514`). Wayland application teardown destroys
   plugins before Workspace, compositor, input, and WaylandServer
   (`src/main_wayland.cpp:106-129`), so backend/workspace are live during plugin
   destruction. Unregister the D-Bus object and service first, then disable the
   controller and remove only mapped outputs; QObject-context zero timers are
   cancelled on destruction. The live order at
   `qindaqtkwinplugin.cpp:140-148` is correct and the seam destructor's second
   cleanup is idempotent because the map is already empty.

8. **Signals/generation.** A virtual add emits `outputAdded` before its final
   configuration/UUID/priority truth, then `outputsQueried`. Workspace handles
   the latter with `updateOutputConfiguration(); updateOutputs()`
   (`workspace.cpp:1294-1298`), while a successful configuration already calls
   `updateOutputs()` (`:509-552`); `updateOutputs()` emits `outputsChanged`
   unconditionally (`:1433`). One hotplug can therefore generate duplicate or
   transitional signals. Sample on a coalesced end-of-turn refresh and compare
   the complete canonical projection; never increment per signal. Shell
   visibility also schedules a zero-timer on output/order changes
   (`kwinshellvisibilitypublisher.cpp:100-106`,
   `shellvisibilityrefreshscheduler.cpp:18-37`), so both publishers must use the
   same settled output projection/generation rather than expose an intermediate
   synchronous lead.

9. **Field truth/order.** LogicalOutput delegates `geometryF`, scale, name,
   manufacturer, model, UUID, physical size, internal, and refresh directly to
   BackendOutput (`src/core/output.cpp:395-495`); priority exists only on
   BackendOutput. UUID is assigned by the configuration store after backend
   discovery and can change on hotplug
   (`backendoutput.h:140-145`, `outputconfigurationstore.cpp:869-888`). The
   no-real-output Placeholder has name `Placeholder-1`, valid mode/scale, but
   empty UUID/metadata and default priority 0 (`src/placeholderoutput.cpp:12-31`)
   and is deliberately in Workspace inventory (`workspace.cpp:1331-1345`).
   Preserve empty UUID as truthful unknown; do not invent identity.

10. **Arbitrary/equal priorities and observation limits.** Protocol input
    assigns any `uint32_t` priority without range/uniqueness validation
    (`src/wayland/outputmanagement_v2.cpp:290-297`); Workspace stable-sorts
    ascending and therefore preserves backend order for equal values
    (`workspace.cpp:2600-2614`). Preserve the full `quint32`, do not cap it by
    output count or treat it as identity/index. Observation bounds must be the
    existing shared visibility bounds: at most 64 outputs and scale `(0,16]`
    (`src/shell_visibility_protocol/include/qindaqt/shell_visibility_protocol/wire_limits.h:11-17`).
    Narrow add-request bounds (8 owned, smaller scale/dimensions) are a separate
    mutation policy and must not reject an otherwise representable pre-existing
    inventory. The live inventory now references those shared 64/16 constants
    and does not constrain priority (`kwinoutputinventory.h:56-68`,
    `kwinoutputinventory.cpp:144-191`).

## Requested next action

Repair the distinct output gate and logical-size naming/contract, retain the
request-name QPointer map and post-bus teardown order, then cover the marker
matrix (inherited/wrong/marker-only/general-only/scenario-only/all-three),
non-virtual valid+malformed identical rejection, prefix collision, synchronous
QPointer nulling, equal/`UINT32_MAX` priorities, and observation boundaries
64 outputs/scale 16 in the authorized test lane. This audit makes no compile,
runtime, nested, DRM/GPU, monitor, or physical-hotplug claim.
