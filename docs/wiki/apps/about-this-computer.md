# QindaQt Settings — About this computer route

`qindaqt-settings --page about-computer` is a read-only system-info surface:
hostname, hardware, kernel, operating system, QindaQt version, the exact
installed Portage checkpoint, disk and memory usage, and battery charge and
health. Unlike every other route this wave, it has no mutation authority and
no live subscription — a `refresh()` action re-reads every source once, and
a `copyReport()` action copies a plain-text summary to the clipboard.

## Sources

Every source is a synchronous, best-effort, read-only fetch, gathered once
at page construction (and again on `refresh()`):

| Field | Source |
| --- | --- |
| Hostname, hardware vendor/model, chassis, kernel, OS pretty name | `org.freedesktop.hostname1` (system bus, `Properties.GetAll`) |
| QindaQt version | The compile-time `QINDAQT_VERSION` string, same constant every other QindaQt executable already carries |
| Installed checkpoint | `/var/db/pkg/gui-wm/qindaqt-desktop-*/PF` — Portage writes the exact installed atom string there. Gentoo/Portage-specific; a non-Gentoo host or unreadable database reports unavailable |
| Disk (root filesystem) | `QStorageInfo::root()` |
| Memory | `/proc/meminfo` (`MemTotal`/`MemAvailable`) |
| Battery presence, charge, state, health | `org.freedesktop.UPower` (system bus): `EnumerateDevices`, then the first present `Type == Battery` device's `Vendor`/`Model`/`Percentage`/`State`/`EnergyFull`/`EnergyFullDesign` |

Every source that fails or is unavailable (no system bus, a non-Gentoo host,
an unreadable package database) reports its own "unavailable" state rather
than failing the whole page or guessing a value.

**Battery health reads UPower directly, not the public Power1 client.** The
public `PowerSupply` protocol struct that every other power-aware route
consumes does not carry `EnergyFullDesign` (design capacity), which is
required for a true health percentage (`EnergyFull` / `EnergyFullDesign`).
Extending that shared protocol for one read-only info page was judged out
of scope for this route; UPower's `DisplayDevice` aggregate object does not
carry design capacity either (confirmed live on `qinda-top`: `0` there,
populated correctly on the real `battery_BAT0` device object), so this
route enumerates real devices rather than using the aggregate.

## Composition

A single-file boundary (`about_computer_info_reader.cpp`) holds every
D-Bus/filesystem read behind an `AboutComputerInfoSource` interface; an
allow-list boundary scan enforces that no other file in this route imports
`QtDBus`/`QDBus` or shells out via `QProcess`. `AboutComputerSettingsModel`
wraps one injected source, exposing copied summary strings and booleans to
QML — never the raw `AboutComputerInfo` struct. The QML module owns a
route-local `QML_SINGLETON` composition root
(`AboutComputerRouteComposition`), mirroring every other route's singleton
pattern.

This route's page `Component` (like every other route's) lives in
`SettingsRouteComponents.qml` rather than inline in `Main.qml`, which is
already at its file-lines review threshold.

## Verification and stopping point

```sh
ctest --test-dir <build> -R '^qindaqt\.settings-about-computer-' --output-on-failure
```

Report formatting (every-source-present and every-source-unavailable cases),
the settings model (single construction-time read, unavailable-source
summaries, `refresh()` re-reads and clears copy status, `copyReport()`
writes the formatted report to the clipboard), an offscreen accessible page
test (every section renders, the copy-report button updates both the
clipboard and the visible status label), and the standard positive/hostile
boundary scan.

Product and test runs never touch a host D-Bus session or system bus (tests
inject a fake info source; the real `SystemAboutComputerInfoSource` is
exercised only by hand, over ssh, against the real qinda-top values quoted
above). The installed-route relocation row exists in the test matrix but was
not run in this candidate — it needs every other route's QML plugin built,
the integration gate's full-tree build, not a focused lane's.
