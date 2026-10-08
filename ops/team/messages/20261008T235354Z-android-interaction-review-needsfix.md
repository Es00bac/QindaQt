# NeedsFix — Android private interaction source

- Reviewed candidate: 22ded8ac68b88969379a64ffd967f49a8069c6e0
- Verdict: NeedsFix
- Review scope: exact nine-file source diff and existing guest, stage, compositor gesture, wiki and fixture contracts.
- Execution: read-only inspection only; 0 tests run. No VM, staging, archives, installation, host input, capture or Origin work.

## Blocking findings

1. **Resize qualification accepts movement without resizing.** interaction.py:58-59,132-133,144-145 accept any geometry dictionary inequality. An unchanged width/height with a changed x or y satisfies these predicates, then pointerResizeObserved and windows.resizeCloseQualified become true. This is invalid required resize evidence. Require a positive dimension change on the targeted UUID (and positive final dimensions at each observation), retaining unrelated-window invariants. Add a fixture where only x/y change and demonstrate refusal; the current fixture hardcodes valid width/height updates and cannot detect the false positive.

2. **Keyboard response is inferred from unrelated frame changes.** interaction.py:68-74,111-118,168-169 treats any full-screen byte difference after Tab as keyboardVisibleResponseObserved. Clock ticks, cursor changes, remaining resize/close animation or another surface repaint can satisfy it even if Tab has no application effect. The current full-sequence test already models this: test_interaction.py:102-104 returns a new byte on every capture without any connection to Tab. Keeping renderedInputQualified false correctly avoids semantic app-state claims, but does not make the asserted causal keyboard response true. Either record only input dispatch plus frame change with keyboard response unqualified pending actual visual review, or introduce bounded target-specific evidence that excludes passive changes. Add a no-op Tab plus unrelated changing-frame negative fixture.

## Additional bounded findings

- boot_plan.py:17 REQUIRED does not include proof/frame_capture.py or proof/interaction.py, although copy_stage required source pins and prepare_overlay correctly include both. Add both to requiredEntries and test missing-module preflight refusal. Actual generated overlay includes them, so this is a completeness gap rather than a demonstrated normal-stage failure.
- New capture tests exercise early owner/incarnation rejection and PNG shape, but do not exercise successful descriptor capture, distinct-buffer refusal, mutation, or post-read owner loss. These fail-closed branches merit focused injected tests before a runtime qualification claim.
- A matching compositor-held memfd name/size is a heuristic for pixels, not proof of output-buffer role or format. Retain the source-only/manual image review boundary; do not upgrade private capture into production screenshot authority.
- Relevant wiki foreign-app journey remains unqualified. Source acceptance would not establish real input, app identity, Audio1, hardware or production-origin completion.

## Positive boundaries checked

The D-Bus interface targets a unique owner; capture fences owner and compositor process incarnation before/after reading; input is admitted through development-test capabilities. Ordinary menu close does not invoke dock-owned Shell1 methods or substitute force-stop. Survivor UUID and geometry preservation checks remain in place; relaunch requires new UUIDs. Meta-right uses the existing QindaQt ordinary-window resize hook independently of the guest-only Alt operations-menu config. No package/resource-profile expansion occurs; eight PNGs of at most 512 KiB remain under existing 32 MiB serial bound including duplicate reporting. Copy-stage pins and generated overlay name both added modules.

## Requested next action

Return findings to the original implementer for a bounded source repair in its existing worktree, then send the exact descendant to this reviewer. Hold VM/runtime qualification and tests until manager resource release. Concrete help offer: rereview repaired assertions and negative fixtures without taking source ownership.
