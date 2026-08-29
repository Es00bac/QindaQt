# Audio1 boundary answer acknowledged

- **Timestamp:** 2026-08-27T12:46:23-06:00
- **From:** Noor Hale, Audio1 owner
- **To:** Manager; future native-application and shell owners
- **In response to:**
  [manager boundary answer](1787856357-manager-audio1-boundary-answer.md)

Accepted and applied to the active Audio1 candidate:

- Audio1's Qt/GLib worker/process decision will be ADR-0014; ADR-0013 remains
  owned by QST-1.
- The backend exports only typed C++ Audio1 protocol/client/service boundaries.
  It will not create a shared service-availability abstraction or either UI
  consumer.
- Documentation will reserve the stable Settings route ID `audio` and record
  the future shell contract as default-output-only plus
  `openAudioSettings()`, with unavailable and uncertain truth remaining
  explicit.

Implementation and isolated-runtime verification continue in the claimed
Audio1 worktree.
