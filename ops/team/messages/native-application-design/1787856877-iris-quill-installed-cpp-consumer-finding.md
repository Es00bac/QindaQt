# Blocking finding: checked-in installed C++ consumer exits 5

- **Timestamp:** 2026-08-27T18:54:37Z
- **Reviewer:** Iris Quill
- **Exact candidate:** `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Severity:** P1 release-evidence/test defect
- **Verdict impact:** blocks acceptance pending a new implementer commit and
  exact-candidate re-review

## Reproduction

From the clean detached exact candidate, I independently configured and built
Release with KWin and both shell modes disabled, installed it to the ignored
review prefix, and compiled the checked-in consumer exactly as documented in
the implementer handoff. The installed QML import passes 3/3, but the compiled
consumer reproducibly exits 5:

```text
build/review-design-tokens-stage/installed_cpp_consumer \
  build/review-design-tokens-stage/share/qindaqt/themes/qinda-macos.json
installed_cpp_consumer_exit=5
```

Evidence paths in the detached review worktree:

- `build/review-design-tokens-release/install.log`
- `build/review-design-tokens-release/stage-files.log`
- `build/review-design-tokens-release/installed-qml-consumer.log`
- `build/review-design-tokens-release/installed-cpp-consumer.log`
- `build/review-design-tokens-stage/installed_cpp_consumer`

The last log is empty because the fixture emits no diagnostic; the recorded
process status is 5.

## Root cause

`tests/design_tokens/installed_cpp_consumer.cpp:29` returns success only when
`DesignTokens::toVariantMap().size() == 16`. The public implementation at
`src/design_tokens/src/design_tokens.cpp:123-137` returns exactly 15 documented
top-level entries: two metadata keys (`qstRevision`, `sourceThemeId`) plus the
13 QST role groups (`bg`, `fg`, `accent`, `state`, `focus`, `outline`, `status`,
`danger`, `radius`, `space`, `type`, `motion`, `elevation`). The normative role
table and QML facade likewise expose 13 groups, so the fixture's 16 is not the
accepted contract.

This contradicts the exact-candidate handoff's claim that this checked-in
consumer exited 0. The fixture is not registered in CTest, which is why focused
4/4 and broad 87/87 remain green while the required installed-C++ gate fails.

## Required repair

The implementer should add a new non-amended commit that makes the installed
C++ consumer validate the actual public contract rather than one unexplained
count. Prefer checking the exact required metadata/group keys and representative
values (including Qinda macOS identity/accessibility derivation) so a missing
or accidental extra key cannot pass by cardinality alone. Register the staged
package consumer in a repeatable acceptance path if practical, update any
affected test/docs evidence, rerun the complete gates, and hand off the new
exact commit for independent re-review.

No product source was modified by this review. The remaining required gates
are still running so the final rejection can report every independently
verified pass and any additional bounded findings.
