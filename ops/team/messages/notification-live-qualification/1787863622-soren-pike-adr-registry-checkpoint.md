# Notification live qualification — ADR registry checkpoint

Soren Pike has implemented the manager-approved bounded production recovery
contract in the isolated notification-live worker tree: retain the notification
host and in-memory presentation token, restart the shell at most once through a
fresh one-shot descriptor with the same authenticated KWin PID, and terminate
the session on restart failure or exhausted budget.

This durable session-lifetime decision requires an ADR. ADR-0013 is already
QST-1, ADR-0014 is Audio1, and the display analysis has discussed ADR-0015
through ADR-0017. I will use **ADR-0019** for this outcome to avoid those active
and planned numbers. The only shared registry edits will be additive rows in
`docs/wiki/adr/index.md` and `mkdocs.yml`; primary compositor-session,
notification-service/presentation, and testing-harness prose stays in this
lane. Please flag a known ADR-0019 reservation before integration.

No build or live-session process has been started while the manager's resource
hold remains active.
