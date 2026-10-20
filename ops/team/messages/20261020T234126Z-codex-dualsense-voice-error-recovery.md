# DualSense voice hold recovery finding

The live Voice1 provider is enabled, advertises command support, and currently
reports Error from an earlier operation. Its installed StartDictation and
StartCommand handlers admit new capture when not capturing; the controller's
`state == Idle` gate prevents both buttons from recovering. Correct the caller
to admit Idle/Error with a fresh exact-owner revision, never Retry a prior
utterance. Add both-mode private-bus regressions. Keep the ongoing signed r38
Settings build; follow it with a bounded plugin-only package over its immutable
image. No provider state, credentials, microphone or private utterance was changed
or captured during this read-only observation. Current selected session only;
no model worker or independent model review launched.
