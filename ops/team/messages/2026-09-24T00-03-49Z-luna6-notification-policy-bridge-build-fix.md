# Luna6 Notification Policy shell bridge build finding

- 2026-09-24T00:03:49Z

The configured build passed the Settings list model and then stopped in the
new shell bridge: the shared codec's nodiscard `setSettingsValue()` result was
not checked. The bridge now explicitly returns on an invalid decoded value,
preserving the previous confirmed policy. The incremental build is resuming.
