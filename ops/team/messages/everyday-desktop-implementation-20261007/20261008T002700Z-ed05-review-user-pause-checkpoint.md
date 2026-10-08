# ED05 read-only review safe stopping checkpoint

- User requested a safe stopping point for logout/login within a couple of minutes; root relayed the instruction. This explicitly pauses refill/review work.
- Exact candidate reviewed so far: `41165258389ad58922b2ea4f73514317f9cf07af`, base `3859cc785fa5dfba7f994a4231d6a8a9842a52c1`.
- Reviewer: everyday_media_delivery, different author from Astra implementer.
- State: **INCOMPLETE REVIEW; no source acceptance or native qualification**.
- Own edits: stable worker board and this checkpoint only. No Astra product/docs/test edit, ED11 source change, Viewer repair, compiler/native/private-bus/display/service/device/package action.

## Inspected before pause

Confirmed exact clean source HEAD, candidate changed paths and freeze receipt. Read the owning recovery/ADR0355/Accepted failed-copy preservation contract and inspected the actual four private collaborator header/implementation pairs: recovery_types, recovery_record_codec, recovery_record_store and recovery_mount_admission.

The inspected source shows canonical bounded JSON re-encoding, decimal64 values, immutable operation and phase sequencing, append-only exclusive/no-follow 0600 records, subtractive syscall fault callback, real file/directory fsync and readback, and move-only pinned Linux statx directory admission with fresh no-follow pathname checks. These are inspected branches, not executed durability or fault evidence. No whole Move/retirement/index/restore integration is delivered or accepted by this first slice.

## Unexecuted durability concern to resume first

Exact path `src/apps/file_manager/mutation/recovery_record_store.cpp`, append lines169–198: file fsync completes and fileSynced is set before the SyncDirectory fault callback; final held/named stat baselines are captured only afterward, followed by canonical readback. A same-inode external truncate/rewrite to the exact expected record during that callback window may leave current file data changed after the acknowledged file fsync while readback still matches. This deserves a real focused fault/interleaving fixture and careful durability classification; it is **not a reproduced or final classified blocker** at this checkpoint.

Proposed bounded fixture: at SyncDirectory, open the actual just-created record without replacing the name, truncate/rewrite the exact canonical bytes through that same inode, perform no positive file-durability substitute, then observe append status and flags. A success must establish that the bytes/version it accepts are covered by a file durability acknowledgement or explicitly fence the detected change. Preserve original/fixed results and do not imply a syscall sequence excludes later external writers. The remaining statement about power-loss durability requires the owning filesystem contract, not a fake fsync success.

## Remaining review and gates

Actual authored record/mount fault fixtures, complete SafeTreeAccess helpers, lifecycle edge cases and independent documentation/boundary checks have not been fully inspected/run. No compiler or native lease was acquired. Platform's Windows resources were not disturbed. Resume at this exact candidate or its explicitly supplied descendant, inspect the untouched fixtures, complete source classification, then schedule the two focused native targets and owning Copy/local/controller/HomeTrash regressions after root grants resources.

Viewer exact15eec source NEEDS_FIX review807891f19 remains open; root owns old/fixed keyboard and cancel→zoom tests and repair. Printing exact551 source/native handoff remains unchanged; root owns acceptance/integration/adoption. User pause overrides the normal next-compatible-packet loop.
