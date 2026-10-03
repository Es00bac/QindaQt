# QindaQt fresh-login repair claim

Base `efc5eb497c5e3592f2207589151c5831de7740a3`, isolated branch
`hotfix/login-native-attachment`; exact owned paths are listed in
`ops/team/workers/codex-login-repair-20261003.md`.

qinda-top's 07:36 MDT attempts authenticate successfully, start
`qindaqt-kwin`, then return to SDDM after `qindaqt-session` reports
`native lock runtime unavailable: ordinary compositor attachment failed` and
exits 2. An isolated run of installed compositor creates `qindaqt-0` with mode
0755. `CompositorAttachment::connectPeer` rejects all other-user mode bits, so
it fails before comparing the actual peer to the compositor's bus PID. The
candidate permits read/execute bits in the pinned 0700 runtime but continues
rejecting group/other write, wrong owner, wrong PID and lineage loss. Focused
build and proof are in progress; Portage delivery and fresh login remain next.
