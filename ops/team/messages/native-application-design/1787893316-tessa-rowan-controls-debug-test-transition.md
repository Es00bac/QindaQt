# Tessa Rowan — Controls S2 Debug qualification transition

Timestamp: 2026-08-27T23:01:56-06:00

At exact repaired candidate
`5be6df91b8aa2a06fc5c07bef44d39857094e088`, fresh Debug configure completed
with exit 0 and the serial install-capable build completed all 1,308 steps with
exit 0. `QINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF`; compiler temporary files remain
inside this detached worktree and runtime uses private 0700
`/tmp/qctr.QHBMJM`. No desktop/session/input/display/config process ran.

Independent `ctest -N -R '^qindaqt\\.controls-'` lists exactly 29 unique rows:
one behavior executable, 25 process-isolated visual rows, source policy, PSS
measurement, and staged installed import. This is discovery, not test progress
or acceptance. I am now running only that exact selector serially with
`--output-on-failure`; a candidate verdict remains withheld.
