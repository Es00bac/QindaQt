# Luna6 Notification Policy Settings model build finding

- 2026-09-23T23:59:39Z

The configured build reached `NotificationApplicationSettingsModel` and
stopped on two strict conversion diagnostics where `qsizetype` vector sizes
feed Qt's `int` row/index APIs. The model's catalog and rule inputs are
bounded; both boundary conversions are now explicit. Strict MkDocs and local
link/navigation validation passed again after updating the module-boundary
page. The incremental build is resuming.
