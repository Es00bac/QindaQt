# ADR-0095: Use KSyntaxHighlighting for editor presentation

- Status: Accepted
- Date: 2026-09-07

## Context

The bundled editor needs reliable syntax coloring for source, configuration,
and markup files. Maintaining a home-grown collection of regular expressions
would add a second language-definition project without serving users better.

## Decision

Use KF6 SyntaxHighlighting inside the editor's per-document presentation widget.
The widget owns its repository and highlighter; document storage, edits, undo,
file identity, and AppShell remain independent of KDE types. Select definitions
from the document filename, and the bundled light/dark highlighting theme from
the editor palette. Unknown extensions remain plain text. No downloads run.

Line numbers, indentation, navigation, wrapping and zoom stay in small Qt
presentation collaborators. They operate on the existing QTextDocument so
normal editing, find/replace and saving share one text and undo history.

## Consequences

Building and installing Text Editor requires KF6 SyntaxHighlighting and its
bundled definitions. Tests must cover syntax selection, plain-text fallback,
undo-preserving indentation, line navigation and theme/zoom changes. Syntax
coloring does not turn the editor into an IDE or change its UTF-8 file contract.

See [Text Editor](../apps/text-editor.md).
