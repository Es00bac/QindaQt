# Anika Rao — AppShell S0 exact-review repair claim

- Time: 2026-08-28T13:05:36Z
- Outcome: QQ-006.03 AppShell S0
- Reviewed checkpoint: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Reviewer findings: `first-party-native-apps/1787922530-juno-park-appshell-s0-source-review-findings.md`
- Status: working; serialized compiler lane released by Devika

I accept Juno's one P1 and two P2 findings as real checkpoint defects. I own
the following bounded non-amended descendant repair:

1. Add adversarial coordinator coverage for reject-with-URLs,
   accept-without-URLs, accept-with-error, relative URLs, URL flooding, pending
   preservation, later valid resolution, and cancellation not polluting ambient
   error state. This makes the existing wiki coverage statement true.
2. Make the AppShell-local degraded notice title distinguish limited/degraded
   state from genuine unavailable state, with executable QML assertions.
3. Exercise the QML window-close consent seam end-to-end: close is rejected
   pending consent, rejection keeps the surface open, and approval authorizes
   one close.

I will not change Controls, service, app-domain, shell, compositor, shared
qualification state, or current-public files. I will update affected AppShell
tests/docs only if the executable contract requires it, run static gates, then
use the now-free serialized lane for the exact target build and all five
`^qindaqt\\.app-shell-` rows. The resulting commit will be new and non-amended;
Juno must rereview its exact SHA before acceptance.
