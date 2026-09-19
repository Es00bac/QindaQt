# QindaQt Settings — Default applications route

`qindaqt-settings --page default-apps` chooses the applications that open web
links, mail, folders, text, images, PDFs, videos, and music. It uses the public
installed-application catalog and local KConfig files. The route has no daemon,
D-Bus authority, or subprocess.

## Packaged desktop defaults

The session package installs `share/applications/qindaqt-mimeapps.list`.
`XDG_CURRENT_DESKTOP=QindaQt` selects these distribution-level fallbacks through
the [freedesktop MIME Applications lookup order](https://specifications.freedesktop.org/mime-apps/latest/file.html).
User and administrator choices keep their higher precedence. Session startup
never creates or edits a user `mimeapps.list`; older user choices, including a
previously seeded file-manager preference, remain user policy.

| Category | Packaged default | MIME types written on an explicit choice |
| --- | --- | --- |
| Web browser | Firefox, when installed | `text/html`, `x-scheme-handler/http`, `x-scheme-handler/https` |
| Mail client | Thunderbird, when installed | `x-scheme-handler/mailto` |
| File manager | QindaQt File Manager | `inode/directory` |
| Text editor | QindaQt Text Editor | `text/plain` |
| Image viewer | QindaQt Viewer | `image/jpeg`, `image/png`, `image/gif`, `image/webp`, `image/bmp`, `image/tiff`, `image/svg+xml` |
| PDF viewer | QindaQt Viewer | `application/pdf` |
| Video player | QindaMPV | `video/mp4`, `video/x-matroska`, `video/webm`, `video/mpeg`, `video/x-msvideo` |
| Music player | QindaMPV | `audio/mpeg`, `audio/flac`, `audio/ogg` |

The viewer desktop ID is `org.qindaqt.Viewer.desktop`; QindaMPV's existing
package uses `org.qindaqt.QQMpv.desktop` and executable `qqmpv`. Its associations
cover the eight MIME types currently advertised by that package. Broader media
associations require corresponding QindaMPV desktop metadata support.

Firefox candidates are `firefox.desktop`, `org.mozilla.firefox.desktop`, and
`firefox-esr.desktop`, in that order. Thunderbird candidates are
`thunderbird.desktop` and `org.mozilla.Thunderbird.desktop`. These are optional
installed-handler fallbacks, not package installations or user overrides.
Unavailable desktop entries are skipped before trying the next candidate or
lower-priority preference file.

There is no standard freedesktop default-terminal association; Terminal remains
outside this list. A separate PDF category lets the user keep a different PDF
reader without changing their image viewer.

## Effective defaults and writes

The composition root resolves XDG config/data roots and current-desktop names.
The store receives those explicit ordered paths and one completed public
application scan. It reads desktop-specific then generic files at each level:
user config, administrator config, user data applications, and system data
applications. Each category displays its first representative MIME type; a
partially customized image category is not flattened merely by opening Settings.

Default values are [semicolon-separated desktop ID lists](https://specifications.freedesktop.org/mime-apps/latest/default.html).
The first installed, associated handler wins. Hidden or malformed entries absent
from the public catalog cannot become displayed defaults. Generic-file Added
and Removed Associations are respected at their applicable precedence; a lower
system association cannot remove a higher-priority user desktop entry. The
page reports configured defaults; it does not invent an application ranking
when every preference file lacks a usable default.

A selection normally writes only that category's MIME keys under `[Default
Applications]` in `$XDG_CONFIG_HOME/mimeapps.list`. If a higher-priority
user desktop-specific file already defines any key for that category, the
selection updates that category there so it takes effect. It never modifies
administrator or distribution files. The target is reparsed before writing;
other categories, partially split MIME choices, unmanaged keys, and Added/Removed
Associations are preserved. An installed application whose representative MIME
association has been removed is rejected without modifying those associations.

“Use inherited default” removes the selected category's override in its owning
user file. The store reloads effective preferences after every successful write,
so a revealed lower-priority default is displayed immediately. Inherited values
are never copied into user policy as a side effect of choosing another category.
Errors leave the last confirmed projection available and are exposed to the page.

## Candidate applications and presentation

Candidates come from
`QindaQt::ApplicationCatalog::scanApplicationDirectories()`, the same public
installed-desktop-entry scanner used by File Manager's Open With picker
([ADR-0164](../adr/0164-shared-application-catalog-and-file-manager-applications-browser.md)).
The composition root scans once. The catalog’s suffix-free launcher IDs are
converted to `.desktop` MIME IDs at this route boundary. A candidate declares at least one MIME type
in the category in its retained `[Desktop Entry]` `MimeType=` field. The store
checks that the representative MIME association is effective before persisting
an explicit selection.

The QML module owns a route-local `QML_SINGLETON` composition root. QML receives
only the model's copied rows, with one labeled ComboBox per category, keyboard
selection, and accessible current-choice descriptions. Choices save immediately;
there is no daemon transaction or separate Apply step. The
`SettingsAppearanceRuntime` install component carries this route's QML module.
The boundary scan rejects process and D-Bus imports.

## Verification

```sh
ctest --test-dir <build> -R '^(qindaqt\.settings-default-apps-|session\.sessiondefaults)' --output-on-failure
```

Focused C++ coverage includes category-only writes and round trips, concurrent
external changes to other categories, independent PDF/image choices,
semicolon-list fallback, user/admin/package lookup order, removed and added
associations, higher-priority desktop entries, hidden-handler fallback, and
restoring inherited defaults. The offscreen page gate verifies eight categories,
current selections, and keyboard choice dispatch. Session tests prove login
preserves existing user MIME files and creates no new user MIME policy.

The packaged-defaults test stages the actual session install component, then
queries every supported core MIME type with `xdg-mime` inside temporary XDG
roots. It also covers optional browser/mail candidates, missing handlers,
user/admin overrides, desktop-specific precedence, and QindaQt-only scoping.
Its controlled desktop-entry fixtures are never launched. These gates do not
claim a live file-open launch through a deployed desktop session. The separate
installed-route test requires a complete Settings build with every route plugin.
