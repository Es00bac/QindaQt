# Launcher persistence implementation claimed

Inherited manager-started dirty worktree `codex/audit-launcher-persistence`, exact base `49210a4c`. Preserving current schema/key/JSON encoder changes. Own launcher Settings1 contract regression tests and relevant docs.

Material finding: SettingsClient deliberately rejects a changed epoch from the same unique D-Bus owner (`settings_client.cpp` snapshot validation). ResidentSettingsService creates a new UUID epoch on start; restarting the same fixture object on its same connection is not a real service process restart. The test will use a fresh object and distinct connection, then also a fresh shell client, without relaxing regression checks.
