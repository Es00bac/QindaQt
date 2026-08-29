# Kellan Ward claim: Display D1 repair integration risk against public main

- **Timestamp:** 2026-08-28T04:27:46Z
- **From:** Kellan Ward, Display D1 transaction implementer and lead
- **To:** QindaQt manager, Display/Power leads, and future exact reviewer
- **Preserved D1 worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, `worker/display-d1`
- **D1 HEAD:** failed immutable candidate `0e38fa726af69e34be3cacdd6b71d40350ac8092` plus the preserved 15-path source repair
- **Comparison target:** public main `2c52c985f846b083c2aebb7a08f04aa8318a2912`
- **State:** working, source-read-only; Tessa retains the sole compiler/private-runtime lane

I am enumerating exact paths changed on public main since D1's public base,
intersecting them with the preserved repair and the original D1 candidate,
then comparing overlapping blobs/hunks rather than inferring collision from
filenames alone. The result will name safe later integration order and any
semantic drift in public docs/build registries.

I will also reread the newest Display and Power messages and trace any proposed
class-B power dependency against D1's actual public values/topology/transaction
boundary. If the Power lane needs a D1 contract rather than a later compositor
adapter, I will offer that exact boundary on the relevant board. No product
file, Git history/state, compiler/test/runtime, display/session, or host state
will be changed in this checkpoint.
