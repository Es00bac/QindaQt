# Appearance exact repair compile/runtime gate is green

- From: Turing the 2nd
- To: Katherine Cho, Maxwell the 2nd, First-party workgroup
- Time: 2026-08-28T09:49:06-06:00
- Thread: First-party Settings / Appearance S0 exact repair

## Material evidence

The repaired dirty tree now passes the complete focused compiler/runtime gate:

- serial build: five required Appearance/Settings targets plus the changed
  migration target, exit 0;
- direct QtTest: values 7/7, preview 8/8, model 11/11 ordinary plus 6/6
  adversarial, page 9/9, migration 10/10;
- registered CTest: Appearance 4/4, Settings application 5/5, migration 1/1;
- the Settings application gate includes both bounded build-tree routes and a
  clean component-only staged install with host display, Wayland, QML import,
  library-path, and user-config inheritance removed.

The source repair covers all of Maxwell's P1 findings and the hostile P2/P3
regressions, including user-theme merging without hiding built-ins, per-key
dirty rebasing and result truth, strict enum metatypes, conflict Revert, compact
forward/reverse traversal, and relocatable installed QML lookup. I also split
the draft/result model implementation and traversal proof to avoid growing a
new monolithic file.

## Current action

I am running the final source-shape, docs/link, strict MkDocs, marker, and
whitespace gates. If they remain green I will create one non-amended descendant
of `9a495aad63034a5fa02613df7ab0d17b9d920385`, release the compiler lane, and
request Maxwell the 2nd's exact commit rereview with the complete manifest.
