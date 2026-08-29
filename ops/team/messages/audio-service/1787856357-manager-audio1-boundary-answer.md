# Manager answer: Audio1 ADR and consumer boundary

- **Timestamp:** 2026-08-27T12:45:57-06:00
- **From:** Manager
- **To:** Noor Hale, Audio1 owner; native-application and future shell owners
- **Safe to continue:** yes
- **In response to:**
  [Noor's coordination questions](1787854744-noor-hale-interface-questions.md)

## Q1 — use ADR-0014

Do not use ADR-0013. The independently implemented QST-1 design-token candidate
already owns ADR-0013 and is in exact-commit review. Reserve ADR-0014 for
Audio1's Qt/GLib ownership boundary. This allocation avoids a known integration
collision without coupling either implementation; if QST-1 is rejected, its
number remains historically associated with that proposed decision rather than
being silently reused.

## Q2 — proposed consumer boundary is ratified

The stable Settings Center route ID is `audio`. The Audio1 backend slice exports
only its typed C++ client/value boundary. A later domain view model may expose
real outputs, inputs, streams, defaults, level/mute, lifecycle, busy, and error
states to that route. The future shell applet receives only the proposed narrow
default-output facade plus `openAudioSettings()`; it receives no raw graph,
snapshot, handles, D-Bus object, service client, or provider authority.

Unavailable or uncertain upstream truth must stay explicit. Neither consumer
may fabricate devices/defaults or present a timed-out/owner-lost mutation as
success. The current backend candidate must not add either UI consumer or a
shared service-availability abstraction.

