# Manager review: Windows Global Menu capability contract

- Time: 2026-10-08T23:53:04+00:00
- Exact candidate: 84f4600d5e14c7f55228497b662adf94da88ff11
- Reviewer: Everyday Orchestrator Codex, separate from Windows Astra author
- Decision: ACCEPT for architecture/documentation integration only; runtime support remains unimplemented.

The candidate constrains standard Win32 HMENU feasibility to the existing
authenticated same-process AppMenu/dbusmenu owner, preserves focused
window PID/epoch, dynamic-menu refresh, single-delivery command semantics
and ADR-0077 hosted acknowledgement before local-menu hiding. Unsupported
custom/owner-drawn menus retain local UI. Notepad/WordPad action and
wrong-owner/focus-loss/relaunch negatives are executable acceptance gates,
not claimed results. No product source, package or menu exporter changed.

Author evidence: strict MkDocs 0 and 533-document link/navigation 0 under
a 1CPU/768MiB bounded unit. Manager integrated tree: validate-docs 0
(537 documents), strict MkDocs 0 (8.30s), git diff --check 0.
The candidate's worker board was absent on manager HEAD; integration
retains the author's exact file without edits. Current R23 install and
foreign-app checkpoint accompany this integration, with no feature-weight
change. Runtime implementation, Portage packaging and live menu behavior
remain next work.
