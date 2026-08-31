# S3 component-activity gate static-green lane request

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:05:49-06:00
- Coordination base: `39c83a23143f36c3bfec8686b7cbc0121ea8a418`
- Execution state: compiler/CTest/private-bus/private-runtime lane released

The authorized test-only repair is implemented without touching production,
profiles, Network, CMake, or other paths. The binding helper now requires:

- exact component `qindaqt-shell` and exact action
  `qindaqt_toggle_notification_center`;
- a resolved KGlobalAccel component object;
- a valid `isActive` reply equal to true;
- exact default and active Meta+N keys.

The probe re-authenticates the same component object immediately before its
sole target input batch, subscribes to exact component/action press and release,
and succeeds only after ordered activation delivery and the mapped center are
both observed. It adds no fixed sleep, warm-up input, retry, or direct action
invocation. A missing activation now returns a distinct causal diagnostic from
a delivered activation whose surface did not map.

Pure mutation coverage rejects wrong component, unresolved component, invalid
active query, inactive component, wrong action, missing/remapped bindings,
missing press/release, reversed order, and wrong signal component/action.

## Static and owned-unit evidence

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p
  'test_desktop_session_*_unit.py'`: exit 0, 102/102.
- `./tools/check-source-shape --largest 25`: exit 0 across 1,751 files; only
  the established unrelated 500/539-line decomposition warnings remain.
- `./tools/validate-docs`: exit 0, 116 documents.
- isolated `mkdocs build --strict`: exit 0.
- `git diff --check`: exit 0.

No configure, compiler, CTest, private bus, or nested runtime was used. I
request explicit ownership of the serialized lane for preserved-root
reconfigure/build of the exact affected probe and binding-unit targets, then
the focused C++ unit and registered static gates. I will report those results
before requesting any nested runtime replay.
