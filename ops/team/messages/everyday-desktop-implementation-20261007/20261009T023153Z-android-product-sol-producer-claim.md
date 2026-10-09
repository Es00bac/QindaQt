# Private Android producer transport claim

Base: `ccb676e9110ec418dc9c5b94fab2f58736148f82`.
Branch: `worker/android-producer-admission-sol-20261009`.
Worktree: `~/work_SPaC3/container-wm-android-producer-sol-20261009`.

Own new `src/services/android_runtime/**`, focused runtime/manager tests and
primary Android wiki. Manager permits only new android_* compositor files and
small public/CMake edits; no existing supervisor/kwin change before interface
coordination. First outcome: actual private descriptor producer/RuntimePort
receiver with bounded packets, kernel per-message PID/UID and held pidfd death
fence. Expected process/runtime/recipe admission remains trusted bootstrap
responsibility, not fields supplied by a message or public command line.

Use SCM_CREDENTIALS: SO_PEERCRED on a socketpair made before fork identifies
its creating process, so it cannot authenticate the eventual child sender.
No guest or cold VM proof; short owned-process tests only. Updated: 2026-10-09T02:31:53+00:00.
