# Luna6 Notification Policy build finding

- 2026-09-23T23:49:01Z

The configured `-j24 -l24` build reached the new notification policy codec
and stopped on `-Werror=unused-result`: `setSettingsValue()` ignored the
`[[nodiscard]]` result of `setPolicies()`. The implementation now handles
that failure path explicitly, and the incremental target build is resuming.
No live notification or settings state was changed.
