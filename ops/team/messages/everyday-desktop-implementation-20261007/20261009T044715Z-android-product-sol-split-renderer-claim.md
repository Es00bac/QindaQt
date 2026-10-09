# Split renderer lifetime claim

- Time: 2026-10-09T04:47:15+00:00
- Exact base: 4e57be42d9758b06f60c4d0aa480b170a742ff41
- Branch: worker/android-renderer-peer-lifetime-sol-20261009
- Outcome: native KWin peer extraction plus authenticated collector runtime joined to independently held renderer process lifetime.
- Ownership: new compositor/kwin/android_native_window_peer h/cpp, Android runtime public/private lifetime boundary and tests, primary Android wiki/ADR0374, smallest additive CMake seams.
- Guard: existing registry/publisher/supervisor untouched. No title/app_id/PID-property or guest package/task/surface inference. Private descriptors only; no default origin assignment or guest run.
- Resource request: one short focused build and smoke after code; no resource unit active.
