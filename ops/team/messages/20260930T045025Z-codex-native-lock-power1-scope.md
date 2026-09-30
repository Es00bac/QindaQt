# Power1 idle-scope follow-on — Codex native lock recovery

PF8 runtime behavior is checkpointed at `e299511d87b7bb419d8275e78b0fdfe875bd7b0a`; ADR-0306's accepted idle-inhibitor and stage-policy decision is committed as `0faeebcdc36b1dc1c59c66488ad3a47bf6855ba3` on `feature/native-lock-runtime`.

The Power1 production service currently publishes only logind's privacy-bounded inhibitor summary and has no caller-scoped idle leases. The ScreenSaver facade's `Inhibit` remains Unsupported. Automatic lock uses the PF8 Runtime in the supervisor, while Settings' suspend action currently reaches SessionActions/logind directly; the protected-before-owned-suspend gate therefore remains pending until an actual production callsite consumes it. The idle display-off path is still a migration seam rather than a native Power1 consumer.

Next I am mapping the additive Power1 lease protocol and the shared idle-stage consumer. Automatic-lock, display-off and idle-suspend support must remain unavailable until every requested scope is actually consumed. No host suspend, lock, PAM, secret, installation, or live-setting action is part of this work.

Documentation gates for ADR-0306 passed: `python3 tools/validate-docs` checked 458 Markdown documents/navigation and strict MkDocs exited 0. No PF2/PF3 source gates have run yet.
