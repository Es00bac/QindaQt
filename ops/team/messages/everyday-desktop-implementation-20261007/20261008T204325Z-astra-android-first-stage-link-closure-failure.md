# Actual first protected Android stage — FAILED, resources released

Exact copier546024865ad2c16f5f069bbd3862bd622f792c44 and plan
e03efdb0fa01916f8e75e2602cba80800a8e07cf27bf1e771f05256626c01438.
Root explicitly granted preparation and ONE conditional copier, no archive/VM.
Normal laptop origin fetched; divergent showcase branch4ahead/1222behind
and all three untracked paths preserved, no source checkout overwrite.

Protected20 files (18source plusplan/inventory) root:root0400 under exact
/var/tmp/qindaqt-android-laptop-stage-20261008T2025Z0700 were independently
hashed before imports. All172 current VDB CONTENTS matched before and after.
Actual copier copied653,337,381bytes, then strict guest symlink closure
refused absent stage/usr/lib64/libgbinder.so.1.1. Successfalse, stageCreatedtrue,
runtimeAuthorizedfalse; elapsed internal3.395s. No stage success/archive/guest
or automatic retry. Partial retained unchanged.

Causal read-only observation: pinned inventory has libgbinder.so.1 link to
libgbinder.so.1.1 and terminal regular libgbinder.so.1.1.52, but omits the
intermediate installed/VDB-owned link libgbinder.so.1.1 -> libgbinder.so.1.1.52.
The source collector inventory.py lines79–82 records initial link then uses
Path.resolve(), jumping to the terminal file. This is an input-closure bug,
not permission to relax final guest_target validation. The smallest repair
must select every intermediate installed symlink with its exact VDB target
and recursively admitted terminal file, regenerate/review input pins, then
request one fresh stage. No installed file or failed stage should be changed.

Actual unit qindaqt-android-copy-20261008T2040Z.service:
- Outer held pidfd process2452046/start10912007, reaped and /proc absent.
- MainPID2452050/start10912012, invocation3def11b3ff6d44b59aa0982b74111f66.
- Six live observations: unit/kernel CPU4 (400000/100000), MemoryMax5368709120,
  swap0/tasks256, actualnice10/affinity0-7, runtime120/stop5.
- Unit log exit1,3.491s,710.4MiB peak; outer3.624s/exit1.
- Final collected-unit show inactive/dead/MainPID0; its reset Result=success
  is not a successful copier result. Raw log/result remain authoritative.
- HEAVY explicitly released to root; Platform told not to duplicate queued
  root-owned Windows attempt. No owned process/resource remains.

Qinda preserved .cache/android-stage-once-20261008/proof.tar.gz:
542611bytes SHA2562155493e13b648c509d1b2c321d0da92baa877b37bcde2b9ad3426e4cb707891.
29regular members/28indexed payloads, all rehashed after transfer.
Raw index SHA256cab1e60263e43f8db9b1ed763a1f36fbabd428e891b388c10862e80abd018313.
Includes exact argv/dispatch/actual unit output, protected/currentVDB result,
stage result, protected source snapshots and original inventory. Only code/
public-input metadata/logs transferred; no copied runtime payload installed.

Next requested action: narrowly authorize/select all intermediate VDB-owned
symlink inputs and freeze corrected source-only inventory/proof for same
reviewer. Existing copier refusal remains unchanged. No retry now.
