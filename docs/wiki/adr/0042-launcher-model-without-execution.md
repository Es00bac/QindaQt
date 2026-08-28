# ADR-0042: Launcher model without execution

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Shell launcher lane

## Context

The launcher surface needs installed-application values, deterministic
category/search/ranking, and a launch request boundary before any provider can
scan real menus or any adapter can start processes. Coupling those concerns
now would make the presentation model untestable, let hostile desktop-entry
content reach execution logic, and pre-empt the later Settings1 persistence
route for pinned and recent entries.

## Decision

Launcher L0 is a pure Qt Core model. Desktop-entry text is validated by a
total, deterministic parser into bounded values; a catalog builder claims
desktop-file identities in producer precedence order before parsing or applying
visibility deletion markers. Categories, search, pinned, and recent are
deterministic, locale-independent functions over bounded values. Presentation
sections expose stable label/category identities; a later UI adapter owns their
localized text. Launch requests are `LaunchIntent` values resolved against a
catalog; they carry identity and display data only — never a command line,
environment, or execution path.

Entry scanning (provider adapter), process launch (execution adapter),
pinned/recent persistence (Settings1 route), and localized presentation are
separate future boundaries owned by other modules; none may move into this
model.

## Consequences

- Hostile, malformed, duplicate, and hidden documents degrade the surface
  deterministically and can never invoke code paths that execute anything.
- A higher-precedence hidden or malformed document still claims its identity,
  so a lower-precedence document cannot resurrect the same application.
- Every activation path (keyboard, pointer) must resolve through the single
  catalog intent builder; adapters that construct their own launch values
  break the boundary.
- The model stays exhaustively unit-testable with literal fixture text and
  never touches the filesystem, environment, bus, or processes.
- Until the provider and execution adapters exist, the launcher cannot show
  real installed applications or start them; this is a documented L0 boundary,
  not an omission.
