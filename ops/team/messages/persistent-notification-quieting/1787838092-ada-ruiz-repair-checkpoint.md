# Ada Ruiz completes blocker implementation before broad gates

- **Timestamp:** 2026-08-27T13:41:36Z
- **Preserved candidate:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **State:** uncommitted repair implementation; broad verification remains

All six Rowan P1 findings and both P2 findings now have source plus focused
regression coverage in the isolated Ada worktree:

- real opaque QtDBus JSON is streamed under shared node/byte/depth/container/
  key/string budgets before retaining children; fixed replies are top-level
  bounded and exact;
- client and Qt transport activation are serialized, failures use configured
  backoff, and synchronous transport-start failure publishes Unavailable while
  explicit Retry can recover;
- commit replies are fenced to the initiating epoch/schema/base/key/status
  lineage; changed keys are bounded/deduplicated, subscription relays carry
  captured owner generations, and signals cannot create target-revision loops;
- the mandatory installed QindaQt profile-default document is validated and
  composed before user overrides, with v1/profile/user migration and rejection
  semantics plus real snapshot/user-remove fallback proof;
- a private-bus composition scenario saves through the ordinary controller,
  reconstructs it, reconstructs the shell bridge with the service retained,
  then reconstructs both service and shell from the same isolated file. Policy
  starts fail-quiet, changes only after each fresh baseline, and revision
  assertions prove no replay;
- Qt transport stop removes both the exact-owner relay and local-disconnect
  match; same-client start-ready/stop/start-ready now passes;
- settings and shell offscreen QML exercise Unavailable/Retry/loading/recovery,
  and the normative focused regex now includes the quieting-controls test.

An additional global-revision correctness hole surfaced while adding the
profile remove test: a scoped client previously ignored unrelated-key
invalidations and then committed from a stale repository-global revision. It
now refreshes its commit base on every valid bounded invalidation, with a
far-future/unrelated-key regression that still proves there is no target loop.

Focused Debug evidence passing at this checkpoint includes settings client,
real Qt transport, adversarial Qt transport, protocol/private-bus decoder,
resident service lifecycle, notification bridge reconstruction, settings-app
offscreen, and shell quieting-control offscreen. I am now running the required
full Debug/Release, QML lint, strict docs, source-shape, production build, and
staged install/activation gates before creating one new imperative repair
commit (not amending the preserved candidate).
