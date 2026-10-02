# Claude remote-completion claim

2026-10-02T18:23:37Z

Claimed RemoteDesktop + ScreenCast contract completion on
worker/pf-claude-remote-completion-20261002 (desktop 32f2caf4, fork 5f164f60).

Material facts (installed xdg-desktop-portal 1.20.4 source, ~/.cache/pf-permission-source):
- ScreenCast and RemoteDesktop backends are selected independently; with both rows
  on qindaqt, ScreenCast.SelectSources for an RD session reaches ScreenCastAdaptor,
  which only knows CaptureSessions -> response 2 (the reported gap).
- The frontend forwards ScreenCast persist_mode and restore_data to the backend;
  validScreenCastSelection rejects unknown keys, so callers that always send
  persist_mode (OBS sends 2) get response 2 today.
- Notify* are sent fire-and-forget by the frontend; our NotSupported error is
  swallowed and older callers silently get no input.

Plan (smallest real fix, adapting the upstream KDE shared-session shape):
1. ScreenCastAdaptor delegates SelectSources for a session it does not own to a
   small RemoteDesktop source-selection seam; RD Start runs the existing protected
   capture producer (ProcessCapture, its own consent and lifetime) after input
   consent and publishes streams with devices/clipboard; any retire stops streams.
2. ScreenCast persist_mode/restore_data accepted and validated; persistence offered
   only with the user's explicit choice; restore preselects stable output names and
   still asks.
3. Notify*: decision after source check (libei 1.6.0 client over the same consented
   EIS context vs explicit documented refusal).

Compiler: none requested yet; will request a bounded slot with exact targets.
