# Network installation review — ACCEPT

- Candidate: a77e7ed79f37fcc7670f37b04cf889562fa70a27
- Base: f152d6c9ee04f99c01d4ca07c4dcb46701a42800
- Reviewer: Everyday Review Sol
- Verdict: ACCEPT
- Time: 2026-10-07T17:36:39+00:00

Production scope is cohesive: the owning Network CMake install list includes the omitted NetworkAccessPointActions helper and installs all six physical files under the qml/ paths already named by generated qmldir/compiled resources. No public interface, wire/credential policy or dependency authority changes. New standalone Qt consumer and stage harness isolate import roots and disable both bus addresses, require six Ready components in preferred compiled and forced-disk modes, fail module/each-file withholding, and recover both modes after restoration.

Independent acceptance evidence in the assigned reviewer worktree (build/evidence; generated output not committed):

- Exact candidate source checked out before review. Own fresh `build/network-review` configured strict Debug with BUILD_TESTING=ON, KWIN_PLUGIN=OFF, PRODUCTION_SHELL=ON and KDE_INSTALL_LIBEXECDIR=libexec. Configure/build exit 0; Portage MAKEOPTS directly reports `-j24 -l24` and build uses those exact limits. Nine targeted executables compiled, including independent stage consumer and adjacent Settings tests. One initial command used plural Settings target names and failed before compilation; corrected to declared singular names, final build passed.
- Network focused eight-Ctest selector: exit 0; 8/8 pass, 42 Qt checks and three static/stage rows, zero skipped/failures. Stage log explicitly confirms six types compiled/disk, whole-module expected exit 3, six each-file expected exit 1, disk/compiled recovery.
- Adjacent registry/navigation/search selector: exit 8; 2/3 pass. Combined independent selection is 10/11, not full success. Search reports 4 Qt passes/5 failures: routes 23 versus expected 22 and destinations 6 versus expected 5.
- Exact unchanged base was independently materialized with `git archive f152d6c9...` into ignored `build/network-base-source`, independently configured into `build/network-base-review`, and its route-search executable freshly compiled with strict Debug/same limits: configure/build exit 0. Base route-search CTest exit 8; same five assertions/counts (4 pass/5 failures/0 skips). `git diff BASE CANDIDATE -- src/apps/settings_center tests/apps/settings_center/tst_settings_route_search.cpp` is empty. This proves the optional caveat predates the candidate and does not block this owning-module install repair.
- `mkdocs build --strict --site-dir build/ed-network-review-site`: exit 0; `tools/validate-docs`: exit 0, 514 documents/navigation; `git diff --check`: exit 0.

Caveats: only strict Debug/offscreen/static coverage independently run. New test deliberately borrows installed public dependency modules; this is component loading evidence, not final full-desktop closure, independent dependency installation, Release, hardware/network/radio or physical login qualification. No private bus or live service/installed mutation occurred. Compiler lease is released.

Requested next action: integrate exact candidate and rerun affected gates; retain pre-existing route-search repair as an independently owned follow-up. Send exact final overlay and proposed ED-04 ADR candidates for the next reviews. Reviewer is waiting rather than working until they arrive.
