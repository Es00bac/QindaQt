# Ada Ruiz completes UnknownKey implementation before broad gates

- **Timestamp:** 2026-08-27T12:02:13-06:00
- **Preserved candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **State:** uncommitted narrow repair; broad gates remain

The complete exact-review defect scope now has implementation and focused
evidence:

- repository key preflight returns explicit `UnknownKey` before stale-base or
  revision-exhaustion evaluation, while the service retains envelope then
  EpochMismatch precedence;
- UnknownKey returns unchanged before/after revisions, empty changed keys,
  and exactly empty value/source maps. Mixed known/unknown transactions never
  return partial authority, and `currentAsResult` defensively skips absent
  schema keys so no future path can manufacture invalid QVariant;
- the wire adapter maps the semantic status directly rather than falling back
  through reply-encoding failure to MalformedRequest;
- the client validator accepts exact empty authority maps only for UnknownKey;
  all known-key semantic results still require exact operated-key entries, and
  fabricated value-only/source-only/pair maps are rejected as uncertain;
- repository tests cover unknown set/remove on stale and exhausted bases;
  private-D-Bus tests cover exact epoch/base/status, unchanged revision, empty
  maps/change set, bounded diagnostic, no signal, no file, and unchanged known
  state for both set and remove; public client tests cover both operations and
  contradictory map shapes.

Focused Debug evidence:

- repository/service/client/new commit-validator slice: **4/4 passed**;
- complete `^qindaqt\.settings-` registry: **15/15 passed**;
- `tools/check-source-shape --largest 30`: exit 0, **768 source files**, zero
  violations. The new client-validator behavior is in a separate focused test
  and private-D-Bus assertions are a cohesive helper; the general client test
  remains 463 nonblank lines.

Settings1 reference, service architecture, ADR-0012, status comments, and test
harness now document the exact map shape and status precedence. The shared
desktop-experience coordination, native-app, and platform-service threads were
read; no Settings1 consumer question is pending at this checkpoint.

Proceeding to full Debug/Release, production/QML lint, strict docs/source
shape, staged install, isolated activation, final board reread, and one new
non-amended imperative commit. No live desktop, user bus, compositor, input,
or another worker's source tree was used.
