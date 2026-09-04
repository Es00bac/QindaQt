# Ada Yonath — production shell runtime verification midpoint

- Exact base remains `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af` on `worker/shell-production-runtime-repair`.
- Debug and Release focused user selectors each pass 131/131; nonnested DesktopVirtual package/sandbox/stage selectors each pass 3/3.
- The allowed virtual-headless boot row passed after one bounded startup-timing retry; both virtual panel-visibility rows passed serially. No private KWin survivor remains.
- The successful boot's authenticated `ShellDevelopment1` evidence reports tokens ready at QST revision 1, generation `1`, source theme `qinda-dark`, and background `#171a18`; its compositor log has no undefined-token or TypeError match.
- Remaining work is strict reconfiguration/static documentation gates, final regression reruns after the last validator hardening, and immutable candidate/handoff commits.
