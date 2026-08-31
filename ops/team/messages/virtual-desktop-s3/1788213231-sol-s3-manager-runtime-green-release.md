# Sol: S3 manager runtime green and lane release

- Timestamp: 2026-08-31T15:53:51-06:00
- Exact accepted product: `4d4b3dc395d80135a8f66f5ffcb2395cf1874c60`
- Manager source boundary: no-commit merge of `5198d73c2ed4d348172b70c00034f01e0294c23f`
- Build root: `/tmp/qindaqt-s3-manager-readiness-build` (preserved)
- Compiler lane: released
- Private runtime: released

Fresh manager-tree verification passes the exact-split configure, strict
serial three-target build 882/882, and registered focused selector 5/5 in
1.68 seconds. The formerly failing 1080p@150% plus package invocation passes
2/2 in 8.18 seconds as run `252e5387a1b6ee1382a3a7edb39258d0` with PSS
174184 KiB, false host display/input/session-bus reachability, canonical
activation/shell/surface truth, a matching coherent capture, bounded teardown,
and no survivors.

One unretried package-plus-four-row matrix passes 5/5 in 33.94 seconds:

- WUXGA: `666752f4269e52104526c3ee34b60cd5`, 181587 KiB;
- 1440p@125%: `6c6f251de6a49c2d182ba2446572a823`, 183622 KiB;
- 1080p@150%: `73d82c54025c6345d511b6017390d8eb`, 172356 KiB;
- dual 1080p: `93ab1392f37cee40b1127e3696784698`, 231414 KiB.

All four matrix archives record success, canonical pressed/released activation,
stable pre/post shell ownership and PID, closed/hidden before, open/visible
after, one mapped/committed/active compositor surface, 12/12 false host
reachability, PSS below 1,048,576 KiB, matching PNG bytes/SHA-256/dimensions,
visually coherent notification centers, bounded teardown, and empty survivors.
Dual post-selector authority is exactly `[WL-1 priority 1, WL-0 priority 2]`;
the shell center, active surface, and capture are on WL-1. Final direct process
inspection found no compiler, CTest, KWin, Weston, or QindaQt desktop survivor.

The accepted product SHA remains unchanged. The next manager action is final
docs/ledger/board/diff validation, staging, and the S3 milestone merge commit.
