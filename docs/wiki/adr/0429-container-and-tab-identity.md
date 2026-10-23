# ADR-0429: Give containers and tabs their own identity

- **Status:** Accepted
- **Date:** 2026-10-22
- **Owners:** Compositor, Core, Shell
- **Supersedes:** None
- **Superseded by:** None

## Context

Containers already own a name and color, but the native switcher still exposes
its representative application's caption and icon. Tab labels also change with
application captions. The owner requested explicit container identity and
numbered, renameable tabs.

## Decision

The task list and bundled Alt+Tab display the container name (default
"Container") and a dedicated icon depicting tabs and split panes inside one
outer frame. They use the same resolved accent as container chrome, including
the theme accent when there is no override. A read-only process-local switcher
projection is keyed by KWin window UUID; focus and activation remain native.
Application captions and icons are never rewritten.

A page owns a bounded optional name and a positive default tab number in the
Core value model. New pages receive the next available number; reorder and
member title changes never rename them. An imported page keeps its number
unless that number is already used in the destination. Explicit names survive
page transfers. Blank renames restore the numbered default. Renaming uses the
existing atomic topology transaction. Optional page JSON fields preserve names
and numbers through saved workspace capture and restore; older documents gain
numbers in their stored page order. Core remains free of colors and platform
objects.

## Consequences

The grouped representative continues to carry activation while presentation
belongs to the container. Detaching restores ordinary application presentation.
Tests cover caption independence, recoloring, removal of stale projections,
tab moves and name validation, and legacy and new persistence round trips.
See [Window containers](../architecture/window-containers.md) and
[Hybrid chrome](../architecture/hybrid-chrome.md).

## Revisit when

A native switcher model gains an explicit public container presentation role.
