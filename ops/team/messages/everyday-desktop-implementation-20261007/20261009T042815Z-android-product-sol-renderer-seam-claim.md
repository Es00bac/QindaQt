# Native Android renderer evidence seam claim

Base `00aad665c664c5642a565223194926a3698c0cb6`, branch
`worker/android-renderer-evidence-sol-20261009`, worktree
`~/work_SPaC3/container-wm-android-renderer-sol-20261009`.

Own only new compositor/kwin/android_* plus a small public renderer evidence/
CMake boundary and service authenticated-peer getter. Existing registry,
publisher, supervisor and Windows/FileManager stay untouched. Kernel native
Wayland PID can match an explicitly pre-admitted renderer whose private
credential/lifetime channel is live. A collector/broker process is not implicitly
a renderer. Result is platform origin only, without app/package/task grants.
No default production attachment or real guest success is fabricated. Updated:
2026-10-09T04:28:15+00:00.
