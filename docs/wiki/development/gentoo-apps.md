# Installing the bundled apps on Gentoo

File Manager, Text Editor and Terminal are packaged together as
`gui-apps/qindaqt-apps`. Portage owns their executables, desktop entries, themes
and shared QML libraries. Their dependencies are declared in the ebuild; you
should not need to copy libraries around or keep a development checkout alive.

The versioned ebuild is in `packaging/gentoo/gui-apps/qindaqt-apps/`. Copy that
package directory into your local overlay, run `ebuild <ebuild-path> manifest`,
accept `~amd64` for this package, then install it with:

```sh
emerge --ask gui-apps/qindaqt-apps
```

On the development machine, the existing `qindaqt` overlay is under
`/var/db/repos/qindaqt`. Package-specific keyword acceptance belongs in
`/etc/portage/package.accept_keywords/qindaqt-apps`. Leave the rest of the
system's keyword policy alone.

A release names an exact Git commit in the ebuild. Bump the package version and
source commit together, regenerate the Manifest, and run the focused app tests
before emerging the update. Portage builds with the user's compiler settings;
CMake's component install replaces development RUNPATHs with installed paths.
If local binary-package signing is configured, `emerge --buildpkg` can also
keep a reusable package in Portage's package cache. The source installation
does not require changing the system's binary-package trust policy.

After installation, check `emerge --info gui-apps/qindaqt-apps` and inspect the
package's `CONTENTS` record under `/var/db/pkg/gui-apps/`. `ldd` and `readelf -d`
on the executables must resolve QindaQt libraries from `/usr`, with no reference
to a source checkout. Launch Terminal normally and verify that its prompt is
visible and a typed command produces output. A running Bash process by itself
is not enough.

The package currently includes three apps, not the compositor, session services
or the separate Calendar work. Its configure stage still checks the desktop's
NetworkManager and WirePlumber providers, so those build dependencies are listed
explicitly. [ADR-0096](../adr/0096-package-bundled-apps-with-portage.md) records
this packaging boundary.
