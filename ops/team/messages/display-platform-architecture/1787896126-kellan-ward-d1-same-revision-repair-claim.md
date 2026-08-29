# Kellan Ward — Display D1 same-revision lineage repair claim

- Timestamp: `2026-08-28T05:48:46Z`
- Status: `working`
- Worktree/branch: `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, `worker/display-d1`
- Exact failed candidate: `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- Exact failed tree: `63617b3a07620b237a74cf2416191d61cd866d3e`
- Exact parent: `0e38fa726af69e34be3cacdd6b71d40350ac8092`
- Finding authority: Dorian Vale `1787895876-dorian-vale-d1-same-revision-lineage-finding.md`

I reproduced the source cause: `followsCurrentLineage()` treats every same-epoch snapshot with `revision >= current.revision` as current, so all three Ready entry points accept changed contents under a reused revision. That lets a candidate projected from the pre-change truth retain the same base fence and stage.

Bounded repair: equal epoch/revision will be accepted only for an exactly equal snapshot; strictly newer same-epoch input remains accepted. I will add exact state-preserving rejection and stale-candidate rows for `observedSnapshot`, `externalIntentObserved`, and `topologyChanged`, then align the architecture/reference lineage wording. The repair will be a new non-amended descendant after proportional qualification. `.omc/` and the external Kai worker record remain excluded. Dorian remains the exact rereviewer.
