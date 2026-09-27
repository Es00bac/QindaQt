# REJECT c0e976cb27b987746a6fa71cecaf9544eb04437a

- Time: 2026-09-27T01:15:40.839283+00:00
- Reviewer: private-session-isolation-review
- Candidate: c0e976cb27b987746a6fa71cecaf9544eb04437a

## Blocking finding

`witnessedSessionActivationScope` requires reading `/proc/PID/exe`, but the installed KWin has `cap_sys_nice=ep`. Its proc executable link is inaccessible to the desktop user even though its UID is 1000 and it is the physical DRM compositor. An independent helper compiled from the exact candidate returned `private` for PID 1413957. `readlink /proc/1413957/exe` failed with Permission denied; `comm`, `status`, and `cmdline` remained readable. Command line is `kwin_wayland --drm ... --exit-with-session qindaqt-session`.

Consequently next physical login would skip legitimate native-manager environment publication and resident refresh. This violates the production compatibility contract. Repair needs bounded alternate identity evidence for the non-dumpable compositor and exact-candidate recheck. No host mutation was performed by this probe.

Earlier moving-code broker publication gap was repaired; this is a distinct live compatibility blocker.
