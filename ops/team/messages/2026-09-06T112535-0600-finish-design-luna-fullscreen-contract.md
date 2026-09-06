# Fullscreen rejection contract for KWin adapter

- Worker: finish-design-luna
- Exact base: `90bca1c6`
- Worktree/branch: `.cache/finish-fullscreen-policy` / `fix/finish-fullscreen-policy`
- Date: 2026-09-06

The pure policy repair adds `HybridMemberPolicyPlatform::restoreRejectedPresentation` in `src/compositor/kwin/hybridmemberpolicy.h`. On a competing `maximizedChanged(true)` or `fullscreenChanged(true)` while `m_focus` is already owned, `HybridMemberPolicy` passes the immutable `m_focusBaseline`, rejected `windowId`, and requested `MemberFocusMode` to that seam, then still returns `false` because the native request was rejected. The policy retains the existing focus owner and baseline; a successful rollback leaves `error` empty, while a platform failure preserves the error and still never clears focus state.

The KWin implementation requested from the adapter owner is narrow and must be added in `KWinMemberPolicyManager::Platform` only:

1. Resolve the baseline member and live registry window; fail before mutation if either is absent.
2. For `Fullscreen`, call `setFullScreen(false)` on the rejected window if needed. For `Maximized`, call `maximize(KWin::MaximizeRestore)` if needed. Restore `baseline.member(windowId)->frame` with `moveResize` and clear any transient maximize/quick-tile state that the rejected native request created.
3. Preserve the existing focus presentation owned by another member: do not activate a window, show the rejected peer, reveal shared chrome, or touch any other member. The peer was hidden by the accepted focus transition and must remain hidden while that transition remains active.
4. Run the adapter operation under the policy's existing applying/signal suppression boundary; no `fullScreenChanged`/`maximizedChanged` callback may turn the rejected rollback into a second policy transition.

The adapter must not call `restoreGroup()`, because that would reveal peers and could activate the baseline member. This is also why the policy returns `false` after a successful rejected rollback: the native request did not become an accepted focus transition.

The pure regression `rejectedCompetingPresentationRollsBackWithoutChangingFocus` models KWin's already-mutated signal for both maximize and fullscreen. It asserts the exact immutable baseline, rejected member, requested mode, and that the original focus owner remains published.

The earlier audit's pointer-lock conclusion is withdrawn: KWin 6.6.5 owns `PointerConstraintsV1Interface` and `RelativePointerManagerV1Interface` in `wayland_server.cpp`; QindaQt must not duplicate those protocols. A separate tiny Wayland client qualification can prove native KWin lock/confine behavior later.

Requested action: route this exact seam to the current `kwinmemberpolicy.cpp` owner; keep `kwinmemberpolicy.cpp` out of this worker's pure-policy commit unless ownership is explicitly reassigned.
