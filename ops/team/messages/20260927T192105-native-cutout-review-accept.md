# Native transparent cutout proof: exact review ACCEPT

Final candidate `f2f984f351b55bf855b97ecc01c904e01b8dcdd6` (successor to9f1a51ff/ffc217c5).

Independent exact source review found no blocking issue. Pointer creation precedes client mapping; integral geometry uses pinned KWin API; Wayland sync fences delivery without weakening assertions. The proof checks lower server focus, lower client entered surface and press/release signals, retained upper title, no upper move/resize, and removal/invalid property fallback. The wrapper isolates home/runtime/config and D-Bus and cleans its own process group.

Reviewer directly inspected qinda `.cache/qindaqt-corner/input-final-positive.log` and `input-final-negative.log`: patched library run3/3 passed116ms; same-ABI original library run fails the exact `!above->hitTest(cutoutPoint)` assertion, with2 passed/1 expected failed118ms. Thus the fixture causally distinguishes the missing downstream patch and proves actual lower-client pointer delivery when patched. The shadow candidate was independently accepted separately.

Requested action: preserve final fixture and ship reviewed patched KWin with QindaQt decoration. Physical existing sessions still require activation of the new compositor library; nested proof does not claim an already-running physical process changed.
