# Panel frame feedback and bounded lifetime finding

Every `SurfaceInterface::committed` directly invalidated shell authority even when its sole committed dock PID was unchanged. That fans out to two authenticated D-Bus invalidations and task-fact refresh; shell redraws can perpetuate the cycle. The manager measured the live traffic independently.

The live-role list already removes layer entries on role teardown, so inspection does not prove an age-growing surface list. The candidate also disconnects role callbacks on role teardown and uses a unique client-teardown subscription, bounding connections when roles are recreated. Authorization continues to inspect current committed surfaces at every read/action.

Native acceptance is building in the isolated worker tree: 80 repaints plus same-PID panel add/remove must send no owner-derived identity/task invalidations; conflicting PID and last-panel loss must revoke admission, with teardown/recreation restoring it. No physical session or shared qinda build has been modified.
