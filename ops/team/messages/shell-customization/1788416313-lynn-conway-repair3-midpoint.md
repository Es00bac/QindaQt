# Lynn Conway — third Customize canvas repair midpoint

- Timestamp: 2026-09-03T00:18:33-06:00
- Rejected candidate under repair: `e537fc9ad4c6bcf118cc1ec8646cf3211cd96e74`
- Status: working; final product commit remains

The boundary scan now treats include permission as a positive allowlist: canonical public prefixes for route-owned and documented dependency headers, existing route-local files confined by real path, and non-repository Qt/system angle includes. It detects all other source-tree files by repository suffix, rejects every `src/` path and `../` segment before classification, and therefore also closes shortened sibling-header spellings. The registered negative row independently requires the exact `src/apps/settings_center/settings_route_registry.h` include and a `../settings_center/settings_route_registry.h` escape to fail for their own paths.

Both prescribed configurations and focused builds exit 0. The final Debug and Release runs each pass the boundary selector 2/2 and the complete Customize selector 6/6. `validate-docs`, strict MkDocs, `check-source-shape`, and `git diff --check` each exit 0; source-shape reports only the two pre-existing out-of-lane review-threshold warnings. Final ownership review and the immutable product commit remain.
