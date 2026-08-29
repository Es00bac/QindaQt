# Controls S2 StateCard real-announcement repair handoff

- Author: Cora Vale
- Time: 2026-08-28T05:16:38Z
- Status: exact candidate ready for Tessa Rowan rereview
- Candidate: `e774dac00166d42d7b84cae957944c22f70b02db`
- Tree: `86e8b3ec26ebd62362a0c10ec700d3c5da467afe`
- Direct parent: `a52efb7931f959dab76c62cee0c72f3674495be0`
- Rejected ancestor: `5be6df91b8aa2a06fc5c07bef44d39857094e088`
- Branch/worktree: `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`

The final non-amended lineage preserves all prior repairs. Manager commit
`a52efb7` changes `StateCard.qml` to invoke `announce` through the root
`control.Accessible` attachment. Descendant `e774dac` preserves Cora's focused
test work in `state_card_accessibility_test.{h,cpp}` and
`tst_controls_behavior.cpp`: the exact non-Item/Action warning is fatal, a
`QAccessible` update handler observes construction silence and each real
announcement's root source/message/politeness, and the existing mirror signal
must emit the identical message with the expected status and QML politeness.

Manager-run serial evidence recorded in the two commits is Debug behavior 1/1,
Debug Controls 29/29, and Release Controls 29/29, including behavior, all 25
visual rows, source policy, PSS measurement, and installed import. Cora's final
read-only audit confirms the exact diff is four Controls-owned paths,
`git diff --check 5be6df9..e774dac` and `git show --check e774dac` exit 0, and
the worktree is clean. No broad registry pass is claimed, and no host GUI,
session, or input path was used.

Tessa Rowan: please rereview exact commit
`e774dac00166d42d7b84cae957944c22f70b02db`, specifically the root attached
announcement event, warning-fatal regression, construction silence, and mirror
alignment. Integrate only after that exact rereview passes.
