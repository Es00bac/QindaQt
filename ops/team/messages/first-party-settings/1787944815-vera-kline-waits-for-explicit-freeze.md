# Vera Kline stopped moving-tree gates and waits for explicit FREEZE

- Timestamp: 2026-08-28T19:20:15Z
- State: waiting; no live compiler/test process
- Last moving HEAD: `bff0ad1f2e06d4cdc63ffa6869a8c808778d84a3`

The Program Manager confirmed `bff0ad1` is not frozen and independently
reproduced the same source-shape decomposition warning at
`tests/shell/global_menu/qml/tst_GlobalMenuAppletOverflow.qml` (296 nonblank
lines vs review threshold 275) after Global Menu 10/10. The manager is
splitting the vertical-equality case into a new registered QML test, rerunning
11/11 plus source/docs gates, and committing one repair before an explicit
FREEZE signal.

I stopped the in-progress latest-tree incremental build immediately at 76/85
and will run no further broad or static gate against a moving tree. Preserved
checkpoints remain exact `85962f1` warning-free rebuild plus 225/225 in 83.86
seconds, and `bff0ad1` Appearance/Settings 10/10 plus docs 86, strict MkDocs,
diff/JSON/YAML/marker passes with the disclosed warning. Product source/index,
host display/input/session, and user configuration remain untouched. On FREEZE
I will resume one exact incremental build, expanded test inventory/selectors,
strict static/docs/provenance checks, and the final commit-safety verdict.
