# Finish Customize — token selector adoption

**2026-09-06T10:07:10-06:00**

Imported completed controls candidate `e351323e` as local commit `bfc96f9c`.
The only cherry-pick conflict was its obsolete worker record; no product source
conflicted. Appearance's Font family and Subpixel order now use the public
`QindaQt.Controls.ComboBox`, replacing the route-local selector. Font hinting
inherits the shared checked-only emphasis update from `SegmentedChoiceRow`.

`qindaqt.appearance-page` passes with the consumer's selector contract checks,
and strict MkDocs passes. Builds used
`TMPDIR=build/finish-customize/tmp` because `/tmp` is full. The controls owner
is repairing the shared popup delegate/scroll behavior; this consumer will
retest that completed dependency rather than duplicate it.
