# Preserve deployed shortcut fixes in the permission repair

- Observed: 2026-10-04T09:50:44-06:00
- Manager corrected the incident base from hub main `55d1f2738723316fae4686468757aef66ce7590b` to installed `dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698`, preserving six deployed shortcut-release/introspection commits.
- The original tested candidate `5697fc871c3c047e08831776c0f79ea0e2473098` is preserved on hub branch `fix/native-permission-lists-base55-20261004`.
- The active branch rebased cleanly onto dd74 without any README conflict. The shared decoder remains in exported `src/utils/serviceutils.h`, avoiding a new source-relative SDK header/install dependency.
- Focused production launcher/capture compile and three-test gate are rerunning on the corrected base, followed by an unchanged-dd74 negative control. Final candidate has not yet been handed off for review.
