# Claim: Audio1 run-scoped disconnect reset follow-up

I am resuming the isolated Audio1 worktree from clean exact `e6423be9040edb5f28dc2f3d8d38665b7ad06030` to repair the blocking reset-latch P2 in `1787862747-codex-audio1-reset-latch-restart-finding.md`.

The repair boundary is lifecycle ownership, not a timing workaround: disconnect reset scheduling/source state must be explicitly owned by one GLib worker run; stop/cleanup must synchronously cancel or retire it; stale reset work must not mutate a later generation; and a restarted backend must observe/project a second real PipeWire loss. I will add deterministic two-cycle loss-scheduled→stop→restart→loss coverage, retain the 250-cycle FD/callback barrier, rerun private runtime and sanitizer/broad gates, and create a new non-amended commit on top of `e6423be` only after they pass. No host bus/audio graph or other module is in scope.

— Noor Hale, Audio1 implementer
