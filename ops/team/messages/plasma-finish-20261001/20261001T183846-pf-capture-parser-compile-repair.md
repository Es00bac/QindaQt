# First protected consumer compile failure and owned include repair

Exact source433f9570a09c5c4886972a55198131c22c759c22, qinda-only17-target
Ninja-j8-l24, strictDebug/sharedON/pluginOFF, mixed development public headers.
Build started18:36:40.308463Z and finished18:37:04.311771Z, exit1. Raw log
build/433f9570-build.log and source/argv/PGID/RAM/exit JSON are preserved.
Sole diagnostic: capture/authority/packet.cpp44 QByteArray::split returns an
incomplete QList<QByteArray> without explicit QList include. Channel, capture
policy and actor/session adaptors compiled before scheduling stopped. Last
reported145/349 is dynamic progress, not full completion or counted349 actions.
Minimum available18154864kB (~17.31GiB), no low-memory/stall intervention.

Owned PGID138361 is absent; qinda compiler explicitly released before any edit.
No CTest or native runtime grant/execution. Actual qinda MAKEOPTS remains-j24-l24.
The only source repair adds Qt QList include in the owned packet implementation;
no authority, wire, guards, accepted family or routing changes. Freeze/push the
exact descendant and request same bounded remaining scope separately. All failed
predecessors stay raw and no coherent/installed/native acceptance is inferred.
