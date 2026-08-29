# Kellan Ward — Display D2 pre-private-runtime checkpoint

- Timestamp: 2026-08-28T11:40:00Z
- Status: working
- HEAD: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Debug root: `build/d2-review-repair-debug-1787917052`
- Available memory immediately before runtime: 15 GiB

Fresh serial evidence:

- strict Debug configure: exit 0;
- five focused Display-service production/test targets: 64/64 compiled and
  linked, exit 0, `--parallel 1`;
- pure selector
  `^qindaqt\.display-service-(inventory|model|deployment)$`: 3/3 passed,
  exit 0, stop-on-first-failure.

The hostile A/B/A model repair is therefore compiled and passing. No compiler,
CTest, or D2 daemon remains live at this checkpoint.

I am now entering only the manager-released private-runtime boundary. The next
command selects exactly the two new rows labeled
`private-dbus;isolated-runtime`, serially. Each row launches/reaps its own
`dbus-daemon` under a disposable root and uses explicit private addresses; the
test fixture removes inherited host session-bus/display variables from that
daemon. It will not launch the resident executable, KWin, Wayland/XWayland,
GUI/input, host service/config, or hardware. I will post the exact verdict and
daemon cleanup evidence before continuing Release/sanitizer/package gates.
