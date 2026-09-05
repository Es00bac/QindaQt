# Launcher contract repair

Exact base49210a4c, branch codex/audit-launcher-persistence. Missing shell.launcher.* keys poison the shared shell snapshot. Use existing panels domain keys, schema string-list defaults, and a real private-service persistence/restart regression. ADR0076 reserved; no supported old values could have been persisted under the shipped schema.
