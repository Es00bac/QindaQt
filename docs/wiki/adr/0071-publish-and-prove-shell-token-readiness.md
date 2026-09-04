# ADR-0071: Publish and prove shell token readiness

- Status: Accepted
- Date: 2026-09-04
- Deciders: QindaQt architecture group
- Scope: production shell and shell-preview QML composition

## Context

The QST-1 singleton is process-local. First-party applications publish their
selected theme before loading Controls, but the production shell and preview
previously loaded hosted applets without publishing that singleton. Controls
then received undefined style values; permissive QML execution reduced the
panels to bare text while emitting many warnings. A screenshot can expose this
failure, but cannot prove that the live production process published the
singleton before the first applet.

## Decision

The production shell and shell preview own a small token-publisher collaborator
at their composition boundary. It imports `QindaQt.Tokens`, obtains the
process-local singleton, publishes the selected `ThemeCatalog` entry before any
panel or hosted-applet QML is created, and republishes after a selected-theme
change. Initial publication is a startup prerequisite. A later publication
failure terminates the process explicitly rather than retaining a partially
styled desktop.

The development-only `ShellDevelopment1.Snapshot` schema remains version 1 and
adds a required `tokens` object with readiness, QST revision, publication
generation, source theme, and one concrete canonical color. Boot validators
reject absent, malformed, or unready token evidence. This field is read-only
and does not expose the token object or create a production mutation surface.

## Consequences

- Every process that renders QST Controls must publish its own facade before
  constructing those Controls; Controls never self-publish or find a global
  service.
- Production-dispatcher rows run with fatal QML warnings and inspect a concrete
  hosted-applet token. Nested boot rows independently require live token
  evidence from the authenticated shell process.
- Theme changes update the existing singleton in place, so already-created
  Controls observe one monotonic publication generation.
- Development evidence clients must understand the complete required `tokens`
  object even though the surrounding snapshot schema number is unchanged.

## Alternatives rejected

Publishing from each hosted applet duplicates theme policy and permits panels
to exist before the first applet succeeds. Letting Controls synthesize fallback
values hides composition defects and can mix theme generations. Treating a
screenshot or absence of warning text as the sole guard does not establish the
live process ordering invariant.
