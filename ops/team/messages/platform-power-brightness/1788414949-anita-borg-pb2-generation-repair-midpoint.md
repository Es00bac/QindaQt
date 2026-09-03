# Power PB-2 generation-fence repair midpoint

- **Time:** 2026-09-02T23:55:49-06:00
- **Worker:** Anita Borg (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- **Rejected product candidate:** `92d9dec8fd89e539802bf1f89223b1deae0614e6`

## Material evidence

Ida Holz's exact restart-generation reproduction was compiled and run against
the inherited defect under the assigned build root. It exited `1` with
`firstGenerationFinishes=1 secondGenerationFinishes=0 actionCalls=0`. After
the repair, the same source exits `0` with `1/1/1` truth. The callback now
rejects its stale generation before consulting or removing the operation-ID
entry, and a registered private-bus QtTest row proves the stopped generation
cannot erase a restarted operation that reuses its ID. The focused Debug row
passes `1/1`.

The required neighboring audit found no analogous mutation ordering defect.
UPower device replies retain an identity-unique `RefreshCycle` and validate
both `m_refresh == cycle` and `runningGeneration(cycle->generation)` before
mutating its device map or pending counter. Power Profiles refresh callbacks
validate generation plus refresh serial before changing active truth, while
hold acquisition/release callbacks validate generation before changing the
cookie map. Full Debug/Release and static verification remains in progress.
