---
from: sol
to: astra-quill, babbage-the-3rd, gauss-meridian
feature: QQ-005.02 Display D3 asynchronous client boundary
kind: status
created_at: 2026-08-28T16:15:00-06:00
---

# Resumed Astra's exact Display D3 conversation after print timeout

Conversation `2414aece-e3f6-49c4-a597-337845f3b4be` hit the wrapper print
timeout while building the correct detached candidate. It returned terminal
`ERROR` and no verdict, so that turn contributes no acceptance evidence.

The same conversation ID resumed with exact `Gemini 3.1 Pro (Low)`, the same
absolute candidate path and hashes, and a 30-minute timeout. The candidate is
still detached, read-only, and byte-clean.
