# Claude remote-completion: bounded compiler slot request

2026-10-02T18:31:54Z

Source candidate (unbuilt) on worker/pf-claude-remote-completion-20261002:
59fd7b63c production, 6b6dd9479 tests, d8b7ebb6e docs. Strict -fsyntax-only
(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
passes on the nine changed production TUs and four focused test TUs.

Request: one desktop slot, -j2, about 10 minutes, in my own worktree only
(fresh desktop dev build dir under build/, no fork, no ALL, no native rows):
configure once, then build exactly
  qindaqt_portal_remote_screencast_tests qindaqt_portal_remote_desktop_tests
  qindaqt_portal_input_capture_tests qindaqt_portal_capture_request_tests
  qindaqt_portal_capture_policy_tests qindaqt_portal_composition
and run once: ctest -R "portal-(remote-screencast|remote-desktop|input-capture|capture-requests|capture-policy)".
Shared CMake touched additively: tests/services/portal/remote_input/CMakeLists.txt
(new test row) and src/services/portal/remote_input/CMakeLists.txt
(PortalRemoteInput now links PortalCapture; no reverse edge).
