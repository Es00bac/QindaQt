# ADR-0022: Keep text documents local, optimistic, and atomically persisted

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** First-party applications / Text Editor
- **Supersedes:** None
- **Superseded by:** None

## Context

The first-party editor needs predictable save behavior without importing a
large editor framework or creating a resident document service. A simple direct
write can truncate the only copy after a crash. Blind atomic replacement avoids
partial bytes but can silently erase a concurrent external edit. Mandatory file
locks would make the small editor depend on advisory cross-process policy that
many editors and command-line tools do not honor.

The owning behavior and limits are in
[QindaQt Text Editor](../apps/text-editor.md).

## Decision

Text Editor S1 owns one local regular UTF-8 document per process. It retains a
content digest of the exact opened or saved bytes and uses that revision as an
optimistic precondition for ordinary Save. Changed, missing, or unreadable
targets block normal Save and retain the in-memory buffer. Save As requires
separate consent before replacing an existing destination.

Successful persistence uses `QSaveFile` with direct-write fallback disabled.
The filesystem must provide the temporary-file commit path; a failure leaves
the prior destination in place and is surfaced to presentation as a typed
error. Document state, filesystem persistence, and user-consent presentation
remain separate modules.

Text Editor exposes conventional persistent Qt `QAction` objects through a
normal `QMenuBar`. This is the complete editor command boundary and may later be
consumed by a global-menu exporter. It is not a reusable QindaQt AppShell
framework; common infrastructure requires evidence from a second native app.

## Consequences

- A crash or write failure cannot expose a deliberately direct-written partial
  destination through the supported save path.
- Byte-content changes are detected; metadata-only touches with identical
  content do not manufacture conflicts.
- The design has an acknowledged compare-to-rename race. It prevents common
  accidental overwrites but is not a cross-process lock or collaborative merge.
- Remote URLs, encoding selection, autosave journals, recovery, multi-document
  tabs, and revision history require later explicit outcomes rather than
  widening S1 invisibly.
- Focused tests must prove conflict refusal, destination replacement consent at
  the controller boundary (including the current path), UTF-8/BOM behavior,
  bounds, and atomic-failure-safe results before qualification.
- The action set may grow additively, while published action object names and
  standard-shortcut meanings remain stable within a release so a later exporter
  can consume the ordinary menu tree without an editor-private registry.

## Revisit when

QindaQt accepts remote documents, crash recovery, multi-process collaboration,
or a supported versioned filesystem primitive that can close the precondition-
to-rename race without sacrificing ordinary local interoperability. A second
native application may independently justify extracting a small shared action
or window-composition boundary.
