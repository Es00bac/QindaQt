# Native sleep expanded acceptance repair

The initial immutable 9add7dab run passed 7/7. Expanded c7427d07 run failed: SessionActions fake invalidation was not exported and mutation timeout classified the equal wire deadline rather than the whole-action timer; selected supervisor loss caused a QDBusServiceWatcher self-deletion crash. This is not a completed candidate.

Repaired source 066868aa disconnects/clears facade watcher callbacks synchronously and uses deleteLater because Qt emits multiple owner-loss signals inside one metacall. Owned delay FD closure remains immediate in coordinator/transport stop. Late UID replies require generation; destroyed owners do not replay. Client whole-action deadline precedes wire deadline by 4s (native sleep) or 5s (other actions); fake signals now export their actual interface. Focused shape PASS29, diff-check PASS. Root owns compiler for combined ABI build; repair compilation/private execution await explicit regrant.

Passed rows from the expanded run: selected logind 17/17, NativeLockRuntime 11/11, Qt native request 16/16, both boundary/poison rows. No host bus, lock or sleep action. Needed next gate: exact repaired compile and full seven-row private rerun, then independent review.
