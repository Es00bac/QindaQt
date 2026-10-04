# Permission decoder repair — material finding

- Observed: 2026-10-04T09:40:03-06:00
- The installed KService generic property conversion is unsuitable for the custom XDG permission fields. The repair uses KDesktopFile for the selected entryPath and KConfigGroup.readXdgListEntry through one small shared boundary.
- Native names and every locker/capture file, executable, exact list and private-connection check remain unchanged.
- First executable gate: focused CTest 2/2, Qt checks 20/20 + 7/7, zero failures or skips. Real cache lookups and actual child PID resolution refuse stock-only, absent and empty native fields.
- Manager extended ownership to the two capturelaunchpolicy.cpp list reads and their bounded regressions. Capture source compilation and existing trust/environment checks are now included in the standalone test build.
- Next gate: finish capture metadata/source tests, unchanged-base negative control, diff/identity checks, one pushed commit and independent exact review.
