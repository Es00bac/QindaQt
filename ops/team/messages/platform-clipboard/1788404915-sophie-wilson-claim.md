# Sophie Wilson — Clipboard C1 claim

- Time: 2026-09-02T21:08:35-06:00
- Exact base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch/worktree: `worker/clipboard-service-c1` at `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-service-c1`
- Outcome: bounded `ext-data-control-v1` capture, Clipboard1 protocol/client, resident host, private-bus and fake-Wayland evidence, packaging, and current documentation.
- Material collision: `data/settings/schema-v1.json` and `schema-v2.json` already declare `services.clipboardHistory` with default `true`; this lane requires default-off but explicitly authorizes Settings schema edits only if the key is absent. I will preserve that external authority, make the host fail closed until a confirmed setting arrives, and report the schema mismatch as a bounded integration caveat unless the manager resolves it outside this lane.
