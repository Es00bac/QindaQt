# Claude remote-completion handoff (source candidate, unbuilt under recovery hold)

2026-10-02T18:48:45Z

Desktop branch worker/pf-claude-remote-completion-20261002, base 32f2caf4.
Exact code candidate: 738340091c6816c7355eb93d9471a2ba93799ac5 (this handoff commit adds only ops records).
Fork: unchanged; no fork candidate (no EIS/plugin edit was needed; EIS names untouched
for the root namespace cutover).

Three separate functional slices, each its own commit:
1. Combined RemoteDesktop+ScreenCast: 59fd7b63c (code), 6b6dd9479 (tests),
   d8b7ebb6e + e08bcadde (docs). ScreenCastSourceDelegate seam in capture_ui.h;
   ScreenCast.SelectSources with an RD handle is validated and forwarded once to
   the RD owner (persist refused, as the frontend does); RD Start asks input
   consent, then the same ProcessCapture producer (own monitor choice/consent/
   lifetime), and publishes streams+devices+clipboard_enabled atomically.
   Every RD retirement stops streams; producer close and capture authority loss
   close sessions. Composition builds RD before ScreenCast. ScreenCast and
   RemoteDesktop routing rows must flip together (documented).
2. Notify*: ea54f2ceb. Fire-and-forget frontend forwarding (remote-desktop.c)
   made the NotSupported refusal invisible. First admitted Notify* opens the
   session's own compositor EIS context; module-private libei sender
   (LegacyInput) with ordered bounded queue; absolute/touch mapped through
   published stream rects; ConnectToEIS then refused; Close/lock/disconnect end
   it. Keysym stays NotSupported. New direct dev-libs/libei dependency of the
   resident (already required by the fork): packaging owner must declare it.
3. ScreenCast restore: 738340091. persist_mode adds a default-off "Remember these
   screens" choice; only a ticked choice returns persist_mode + (suv)
   ("QindaQt",1,{outputs: as of stable wl_output names}); frontend owns token
   and PermissionStore screencast row; restore preselects and still needs Share;
   foreign/malformed data ignored; RD never persists.

Changed paths: src/services/portal/{capture/capture_dialog.*,include/.../capture_types.h,
capture_ui.h,screencast_adaptor.h,remote_input/CMakeLists.txt,remote_input/include/.../
remote_desktop_adaptor.h,remote_input/src/{legacy_input*,remote_desktop_adaptor.cpp,
remote_sessions_p.h},src/{capture_policy,capture_wire,screencast_adaptor,
foundation_composition}.cpp,src/capture_sessions_p.h}; tests/services/portal/
{capture/tst_capture_policy.cpp,capture/tst_capture_requests.cpp,remote_input/
{CMakeLists.txt,tst_remote_desktop.cpp,tst_remote_screencast.cpp,
tst_remote_legacy_input.cpp}}; docs/wiki/{adr/0335,adr/0336,architecture/
portal-remote-input,reference/portal-capture}.md.
Shared CMake (additive): PortalRemoteInput now PUBLIC-links PortalCapture (no
reverse edge) and PRIVATE pkg-config libei-1.0; remote_input tests add two rows
(remote-screencast; remote-legacy-input with libeis-1.0).

Evidence actually run: strict -fsyntax-only (-Wall -Wextra -Wpedantic
-Wconversion -Wsign-conversion -Wshadow -Werror) exit 0 on every changed
production TU (remote_desktop_adaptor, legacy_input, remote_sessions,
clipboard_adaptor, input_capture_adaptor, screencast_adaptor, capture_policy,
capture_wire, capture_sessions, process_capture, screenshot_adaptor,
foundation_composition, capture_dialog, helper_runtime, capture main) and the
five touched/new test TUs; mkdocs build --strict exit 0; tools/validate-docs
exit 0 (497 documents). NOT built, linked or run: manager recovery hold
(18:42Z) arrived before any compile. My worktree has a configured build/dev
(configure only, 18:36Z, before the hold; staged fork prefix
pf-manager-capture-production-20261001/stage/usr read-only).

When compilers are released (requested slot: -j2, ~10 min, my worktree only):
  cmake --build build/dev --parallel 2 --target qindaqt_portal_remote_screencast_tests \
    qindaqt_portal_remote_desktop_tests qindaqt_portal_input_capture_tests \
    qindaqt_portal_remote_legacy_input_tests qindaqt_portal_capture_request_tests \
    qindaqt_portal_capture_policy_tests qindaqt_portal_composition \
    xdg-desktop-portal-qindaqt qindaqt-portal-capture
  ctest --test-dir build/dev --output-on-failure \
    -R "portal-(remote-screencast|remote-desktop|input-capture|remote-legacy-input|capture-requests|capture-policy)"

Bounded caveats: tests use fake consent/capture ports and a libeis fake server;
no real frontend, PipeWire node or native compositor row has exercised these
paths (Sol native gate). The remember checkbox is unverified visually.
RemoteDesktop persistence (remote-desktop table) is still not offered (ADR-0335).
Do not label PF19/PF20 complete from this candidate.

Requested next action: root/Sol review the three slices independently; on
compiler release grant the bounded slot above; I repair any finding in this
worktree. I will read my queue and offer help on the same completion next.
