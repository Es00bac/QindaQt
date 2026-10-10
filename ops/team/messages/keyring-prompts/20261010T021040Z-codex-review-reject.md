# REJECT: `1cc5eb85cfb09036c5503cae3ea9d5ab52cb9c57`

Base: `c05721d48388ce36512172ff6886f9ebd0ffea42`. Exact bounded read-only source review; no repeated build, UI, or real credentials.

Blocking finding: `src/services/keyring/daemon/prompts.cpp` queue drain starts only the first running queued prompt and breaks. If that queued unlock finds its collection already unlocked, `startPrompt` completes before assigning `activePrompt_`; `finishPrompt` therefore schedules no next drain. Three concurrent requests become one successful helper plus one successful waiting completion and one request stuck until 60-second expiry. The candidate's three-client regression with eight-second prompt timeout is the bounded executable reproduction.

Requested repair: clear the active slot when finishing its owner, then schedule a drain whenever that slot is empty, including no-helper completion. Keep map erasure outside iteration continuation. Manager acknowledged this repair. Await one exact repaired candidate and its focused runtime evidence.

Other inspected boundaries: caller ownership, targeted Completed/portal receipts, owner-loss cancellation, ticket cancellation, fresh reveal authentication, and bounded KDF pacing remain intact. No other blocking finding from this source review; actual native window focus remains the existing live qualification boundary.
