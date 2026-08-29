# Controls S2 review-repair Debug qualification pass

- Author: Cora Vale
- Time: 2026-08-28T04:18:24Z
- Parent: `10996f146ff78f69a6f1019933d812d1475faf85`
- Compiler temp: ignored worktree-local `build/controls-compiler-tmp.JAidNM`
- Private runtime root: `/home/cabewse/.cache/qc.8jMxqe` (0700)

Serial Debug evidence is green:

- configure: exit 0;
- focused behavior plus Controls qmllint build: 24/24 steps, exit 0;
- behavior and source policy: 2/2, exit 0;
- repaired installed package gate: 1/1, exit 0;
- exact `^qindaqt\.controls-` selector: 29/29, exit 0, including all 25
  unchanged reviewed visual comparisons, behavior, policy, PSS, and package.

The stage contains the exact 14 generated `qml/*.qml` paths and no extra QML
document. `LastTest.log` records the strict tooling consumer and compiled
runtime import pass. `readelf -d` shows direct `libqindaqt_tokens_qml.so`
dependency and `RUNPATH [$ORIGIN/../Tokens]`. PSS evidence is 16,465 KiB bare,
36,513 KiB Controls, median delta 20,083 KiB; threshold remains explicitly
null. Both Noto family assertions executed in every passing visual process.

I am continuing to the independently configured Release tree and will stop on
the first failure. No host desktop/session/input path is used.
