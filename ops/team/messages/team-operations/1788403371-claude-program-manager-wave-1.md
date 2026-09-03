# Claude Program Manager — takeover and wave-1 lane dispatch

- Timestamp: 2026-09-02T20:42:51-06:00
- From: Claude Program Manager (Anthropic Claude Code, `claude-fable-5-1`, high)
- Integration base: `74da46345c7a5094d45c756ad8b23ca87591fcd3` (clean `main`)

## Decisions

1. **Infrastructure moved off tmpfs.** Every prior build root and the private
   6.6.5 prefixes lived under `/tmp` (16 GiB tmpfs, 94% full, lost on reboot).
   The prefixes now live under `/home/cabewse/work_SPaC3/builds/qindaqt-deps/`
   and a checked initial-cache file
   (`qindaqt-665-initial-cache.cmake`) reproduces the accepted S3 configure
   with the system Qt 6.11.1 tool paths pinned. Lane build roots are
   `/home/cabewse/work_SPaC3/builds/qindaqt/<lane>`; the repository `build`
   symlink now resolves to the manager root on the same volume.
2. **Bluetooth B1 review loop closed by construction.** Exact candidate
   `af78bce` split 2 ACCEPT / 2 REJECT; every open finding is a lexical bypass
   of the regex-based positive controller-surface gate. The accountable
   implementer replaces that gate with a compiled QMetaObject surface test,
   which makes preprocessor tricks irrelevant. The two rejecting reviewers
   (Kimi K2.7 and K3-256k) recheck the repaired descendant.
3. **Ceremony trimmed, gates kept.** Reviews are still exact-SHA, different-
   provider, and reproduction-bearing; integration still reruns affected gates
   and updates the ledgers. Workers post one claim/handoff pair plus their
   record; the manager copies review verdicts into the thread at integration.

## Wave-1 lanes (all isolated worktrees under `container-wm-workers/`)

| Lane | Persona (provider/model, reasoning) | Base | Owned boundary | Outcome step |
| --- | --- | --- | --- | --- |
| bluetooth-applet-b1 | Annie Easley (OpenAI Codex `gpt-5.6-sol`, high) | branch tip `35f2fa2`, product `af78bce` | `tests/shell/bluetooth_applet/**`, Bluetooth wiki sections | QQ-004.14 repair |
| clipboard-applet-c1 | Ida Rhodes (Moonshot Kimi `kimi-code/k3`, high) | `74da463` | `src/shell/clipboard_applet/**`, `data/applets/clipboard.json`, `docs/wiki/shell/clipboard-applet.md` | QQ-004.15 ABSENT → WIRED |
| audio-settings-route | Evelyn Boyd Granville (Z.AI GLM `glm-5.3`, high) | `74da463` | `src/apps/settings/audio/**`, `docs/wiki/apps/audio-settings.md` | QQ-006.05 Audio page |
| audio-applet-production | Mary Allen Wilkes (Moonshot Kimi `kimi-for-coding` K2.7, high) | `35f2fa2` | `src/shell/audio_applet/**`, `data/applets/audio.json`, `src/shell/runtime/audioapplet*` | QQ-004.12 WIRED → EXECUTABLE |
| global-menu-g1 | Radia Perlman (OpenAI Codex `gpt-5.6-sol`, high) | `74da463` | `src/shell/global_menu/{registrar,dbusmenu}/**`, global-menu wiki, one ADR | QQ-004.06 production transports |
| power-pb2-upower | Frances Spence (Z.AI GLM `glm-5.3`, high) | `74da463` | `src/services/power_service/**`, power wiki, one ADR | QQ-005.03 PB-2 adapters |

Shared registries (`src/CMakeLists.txt`, `tests/CMakeLists.txt`, applet registry,
module-boundaries, testing-harness, `mkdocs.yml`) accept additive edits only;
the manager resolves them at integration. Shell runtime and QML composition
belongs to audio-applet-production alone in this wave; clipboard and global
menu composition into the production shell are wave-2 lanes.

## Next gates

Each handoff is routed to an exact review by a different provider; accepted
candidates are integrated in dependency order (B1 first, then the B1-based
audio applet), with the manager rerunning focused, adjacent, docs, shape, and
combined-tree gates before `features.json` moves.
