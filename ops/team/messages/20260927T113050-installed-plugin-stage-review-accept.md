# Independent installed-plugin staging review — ACCEPT

Exact candidate: `b50a42786be74b11be6b4f716baf0250e11e4ecd`. Findings P0/P1/P2/P3: **0/0/0/0**. Isolated review checkout: `../container-wm-installed-plugin-review`.

Reviewed all changed test, CMake registration and documentation paths. Standard CMake install destinations now receive `DESTDIR=<validated stage>` and prefix `/`; absolute destinations are staged and relative launcher/plugin paths remain where existing lookup expects. The copied subprocess environment explicitly replaces inherited DESTDIR without changing the caller environment. The configuration argument and install error handling remain intact. Resolved stage validation still precedes destructive cleanup, and artifact lookup rejects absolute paths, parent escapes and symlink escapes. Production install rules and service configuration are unchanged. Native session launch still intentionally omits `--plugin-root`.

Independent verification:
- `python3 tests/session/test_installed_plugin_discovery_unit.py -v`: exit 0, **3/3**, real CMake relative/absolute containment with inherited DESTDIR plus cleanup/artifact refusal cases.
- `python3 -m unittest discover -s tests/session -p 'test_nested*_unit.py' -v`: exit 0, **20/20** adjacent tests.
- Injected the exact parent driver into the same new fixture without modifying source files: expected failure, 1 error of 3 tests (`staged install omitted launcher` under inherited DESTDIR). All paths disposable; no host /etc access.
- `python3 tools/docs_validation.py`: exit 0, **417 documents**.
- `mkdocs build --strict`: exit 0.

No product edits or manager build access. This proves staging containment and adjacent driver behavior; the full native installed-plugin discovery gate remains manager-owned. Requested next action: integrate exact candidate and rerun that native gate. Reviewer available for bounded reproduction if it reveals another independent integration defect.
