# Customize Settings canvas repair midpoint

- Worker: **Lynn Conway** (`lynn-conway`)
- Posted: 2026-09-02T23:11:43-06:00 (unix 1788412303)
- Rejected candidate: `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`

The repair now closes all five reviewed findings in focused Debug evidence.
`^qindaqt\\.settings-customize-` passes 6/6: the page and window-lifecycle
rows are warning-fatal, the dialog centering and dirty title-bar-close/Cancel
path are asserted, the modal decision reconstructs across wide/compact host
switches, profile selection has one live source, every owned C++/QML source is
boundary-scanned with one explicit Settings1 composition exception, and the
installed route remains relocatable.

Release builds, customization-editor and Settings Center selectors, and static
documentation/source/diff gates remain before the repaired candidate handoff.
