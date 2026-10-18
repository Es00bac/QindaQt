# Controller frame stalls: delivered

- Exact implementation: `b331c826c8c283577a1ced0f280001eff123d6d1`, integrated on main.
- Packaging: QindaGentoo `10fd1bbf6c4fb4826ee9ea8ac4c1bb1f41642731`, integrated on master.
- Changed paths: controller runtime/priority inspection, private observation worker,
  controller tests/build entries, controller architecture and ADR-0397; bounded
  r32 recipe, fixed signed-r31 staging helper/public certificate/image ledger.
- Outcome: synchronous same-user FD inspection no longer runs on compositor
  frame dispatch. SDL/input remain on the GUI thread. One normal-priority worker
  coalesces scans, rejects invalidated results and joins on unload. Steam/game
  handoff, immediate invalidation release and neutral resume remain enforced.
- Gates: standalone plugin/controller build exit 0; CTest 5/5, 28 QtTest cases,
  zero failures/skips; strict MkDocs and 573-document local validation exit 0;
  exact plugin imports resolve. Package signature/embedded recipe and 2,652
  retained r31 paths match. Signed Portage build exit 0 in 60 seconds, 1.4 GiB peak;
  laptop binary-package install exit 0 and all 2,374 installed files/symlinks match.
- Artifact: r32 gpkg, 63,313,920 bytes, SHA-256
  `83637df1d16e3af764a4a87027093f1b80ec5475682c5cfc3d2b08ec6b84f5ca`.
- Live: `qindaqt_controllers_r32` loads successfully in the original compositor;
  shell process and saved profile bytes are unchanged. Worker scheduling is
  SCHED_OTHER while compositor stays SCHED_RR. Controllers1 responds without error.
- Timing: initial twelve seconds include twelve 28–29-vblank gaps up to 483 ms
  and 438–454 ms compositor CPU runs at one-second intervals. Final twelve seconds
  have median frame interval 16.667 ms, maximum 33.355 ms, maximum main CPU run
  10.903 ms, no main run above 100 ms. The controller disconnected before the final
  sample, so that comparison alone does not qualify connected-controller playback.
  Deterministic blocked-scanner and virtual SDL tests verify the software boundary.
- Remaining bounded caveat: connected-controller physical playback confirmation.
  The existing Bluetooth helper override is retained; retiring it is separate.
- Next action: retain the installed repair and source/recipe receipts; route a
  reproducible regression to this outcome. No additional queue task or model work
  is started. Primary-session review only, respecting the owner's worker budget.
