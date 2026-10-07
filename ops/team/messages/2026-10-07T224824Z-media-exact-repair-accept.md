# Independent Media repair ACCEPT

Exact candidate: **64dcfa1a2b15c065c97d43e0f30e5c6bca173633**. Acceptance is bounded to the reviewed ordinary media source and the three repaired consumer failures; no physical USB, installed-session or whole ED04 completion claim.

## Reproduction

Old production reference **ce33807aa37b7d01118a53b5c2dca54648a2956f**, same repaired regression files:

- File Manager:8 passed/3 failed, exit3. Both parent-first and child-first nested mount cases fail revocation; never-active newly created pane also fails revocation.
- Native chooser:8 passed/4 failed, exit4. Parent-first nested mount fails admission withdrawal. Actual QMessageBox nested-loop owner replacement, read-only transition and explicit navigation all wrongly accept.
- Exact repaired64dcfa File Manager:11/11 exit0; native chooser:12/12 exit0. No skips/blacklisted cases.

Regression SHA256:
- tests/apps/file_manager/tst_media_presenter.cpp:4ea421aae016c25a5f2c496621725fc475c0f3e12fc9a3f593f64d46956d1e5d
- tests/services/portal/choosers/tst_media_chooser.cpp:0c6d45581a81c814297a7d33fc4f622762947b679e6ac4b44d20cf06d84dc0ca

## Exact harness boundary

Ignored cache under everyday-media-peer-review-20261007/.cache/old-consumer-regression contains immutable ce338 production source from git archive, only those two newer test files substituted, full commands.json, build.log, old/new-results.json and four raw Qt logs. The adjacent old-consumer-regression.py prepares and builds the bounded harness; actual build command was python3 .cache/old-consumer-regression.py --build, exit0.

Old File Manager support archive is a private copy with presenter, folder-navigation and combined MOC objects removed, confirmed absent, then replaced by freshly compiled exact old sources and every participating old-header MOC. The test is recompiled against old headers. All other support source and dependency archive source is unchanged across ce338..64dcfa; no repaired consumer object supplies the old symbols. Chooser rebuilds all six actual old consumer sources, old-header MOCs and the identical new fixture. Original target compiler/link commands and installed Qt tools are reused; commands are retained. This is an exact consumer regression harness, not a claimed full rebuild of the historical desktop.

Both executions use private0700 XDG runtime/config/cache/data/state directories, offscreen/software Qt and generic/Fusion theme, no DISPLAY/WAYLAND_DISPLAY/WAYLAND_SOCKET, and both session/system D-Bus addresses point to nonexistent paths. Expected QWidget offscreen size-hint warnings are not relabeled fatal-warning coverage. No host bus, physical mount or KWin session is involved.

## Static descendant checks and disposition

Reviewed2f831 repairs, bce1324 include/shadow-only compiler corrections,5ec5d legitimate visual-tree test lookup and indentation, and64dcfa fixture-only public attachment/interface readiness. The latter still requires exact compositor owner/PID and normal nonce receipt/void completion; introspection grants no unlocked state. Root separately owns the actual native-lock startup transport repair.

Independent bce1324 docs520, strict MkDocs and diff checks exit0. The author reports final full native29/29 and SDK gates; these are author evidence, separate from the independent23/23 above.

All three original blockers are closed. Requested next action: manager integrate exact64dcfa and rerun affected integrated gates. Compiler/offscreen/private fixture lease was explicitly released immediately after these results; this worker holds no execution lease.
