# Dorian Vale — virtual desktop S0+S1 exact repair rereview midpoint

- Timestamp: 2026-08-28T11:37:35Z
- Exact candidate: `f28f443b7aae2d635481f49e847a7e1e1a3b573b`
- Exact tree: `cf840061b9680df539a150d28db09a6f97a00c6c`
- Exact parent/reviewed FAIL: `fd9faab5ab79017be903dafc6f0587d09c511f49`

Identity and scope pass. The repair delta is exactly the ten declared paths,
+1,027/-163, with sorted repair-path manifest SHA-256
`0f246a74bd2b693871634ceb8b0f340faf5ef956b5566beabb2a0e351271a9ce`.
The complete candidate remains the original 20-path manifest SHA-256
`9dc4cae417408377abc7436fc602edc75fb3f6ee4204f777e187db5b43621a0c`.

All five prior dispositions pass focused source review so far:

- readiness reacquires one complete probe document under one hard deadline and
  validates output/generation, input, dock, and both apps together;
- app ID, nonempty window ID, and title are retained from compositor records;
  the declared process role is consumed without inventing an unavailable PID
  association;
- every attempt has a fresh sentinel-bound result root, artifacts and every
  process/probe log are copied before a result document is written last, and
  timeout/failure/success paths share the finally boundary;
- complete evidence requires exact integer resident PSS and 1,048,576 KiB
  ceiling with nonnegative/over-limit rejection;
- cleanup records every topology role's captured PID/group/path/start ticks and
  only `already-exited`, `term`, or `kill`, explicitly not graceful exit.

Fresh safe evidence: focused Python units 37/37 pass; Python compilation passes;
source-shape 962 files passes; documentation/navigation 58 passes; exact diff
check and clean detached worktree pass. No blocking reproduction is currently
open. I am checking final doc/test correspondence and then issuing the exact
source-safe verdict. The private boot row remains deliberately unrun and cannot
receive live maturity from this review.

