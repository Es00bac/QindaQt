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

| Category | Packaged default | MIME types managed by the category |
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

[ADR-0245](../adr/0245-write-only-supported-default-application-associations.md) records the supported-scope persistence contract.

The composition root resolves XDG config/data roots and current-desktop names.
The store receives those explicit ordered paths and a completed public
application scan. The composition root refreshes that scan on application
directory, desktop-file, and MIME preference changes, with a 15-second fallback
for roots that
were absent when Settings opened.

The store reads desktop-specific then generic files at each level: user config,
administrator config, user data applications, and system data applications.
It resolves each managed MIME type independently. A category displays one
application only when every MIME type has that effective handler; otherwise it
displays a mixed state and preserves each per-type result. Opening Settings
does not flatten a partially customized category.

Default values are [semicolon-separated desktop ID lists](https://specifications.freedesktop.org/mime-apps/latest/default.html).
The first installed, associated handler wins. `NoDisplay=true` handlers remain valid MIME defaults even though they do not
appear in menus. `Hidden=true` deletion markers and malformed higher-priority
entries still suppress lower-root copies and cannot become displayed defaults. Generic-file Added
and Removed Associations are respected at their applicable precedence; a lower
system association cannot remove a higher-priority user desktop entry. The
page reports configured defaults; it does not invent an application ranking
when every preference file lacks a usable default.

A selection writes only the category MIME keys the chosen application
effectively supports under `[Default Applications]` in
`$XDG_CONFIG_HOME/mimeapps.list`. Unsupported keys retain their values.
If a higher-priority user desktop-specific file already defines any key for
that category, the selection updates the supported keys there so it takes
effect. It never modifies administrator or distribution files. The target is
reparsed before writing; other categories, unsupported MIME choices, unmanaged
keys, and Added/Removed Associations are preserved. A handler with no effective
support in the category is rejected without changing the file.

“Use inherited default” removes that category's override across the
desktop-specific and generic user files, revealing administrator or packaged
choices. The store reloads effective preferences after every successful write,
so a revealed lower-priority default is displayed immediately. Inherited values
are never copied into user policy as a side effect of choosing another category.
Errors leave the last confirmed projection available and are exposed to the page.

File Manager's Open With uses the same store one MIME type at a time
([ADR-0269](../adr/0269-open-with-widens-the-bounded-launch-and-karchive-backs-archives.md)).
`loadMimeTypeHandlers` resolves a type exactly as `load()` resolves a
category's types and lists its handlers default first, then the user's Added
Associations, then declaring entries in scan order. `saveMimeTypeDefault`
writes one type's default to the same target a category write would use,
refuses malformed type keys, and also records the application as the type's
first Added Association when its entry does not declare the type (lookup would
otherwise skip the default). Both callers compose the store through
`createSessionDefaultApplicationsStore`, so Settings and File Manager read and
write the same files in the same order.

## Candidate applications and presentation

Candidates come from
`QindaQt::ApplicationCatalog::scanApplicationDirectories()`, the same public
installed-desktop-entry scanner used by File Manager's Open With picker
([ADR-0164](../adr/0164-shared-application-catalog-and-file-manager-applications-browser.md)).
The composition root scans with `ApplicationVisibility::IncludeNoDisplay`
and repeats that scan when installed desktop entries change; other catalog
consumers keep their menu-only default. The catalog's suffix-free launcher IDs
are converted to `.desktop` MIME IDs at this route boundary. A candidate
effectively supports at least one MIME type in the category, considering its
retained `[Desktop Entry]` `MimeType=` field and applicable XDG Added/Removed
Associations. A partial candidate is labeled with the exact MIME types and
count it will change. The chooser and store use the same effective association
projection; an explicit choice writes only that supported scope. When a
category is mixed, the ComboBox shows a read-only “Mixed defaults” state until
the user chooses a candidate or clears user overrides.

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

Focused C++ coverage includes category-only writes and round trips,
concurrent external changes to other categories, independent PDF/image
choices, semicolon-list fallback, user/admin/package lookup order, removed
and added associations, higher-priority desktop entries, NoDisplay defaults,
Hidden/malformed root masking, hidden-handler fallback, restoring inherited
defaults, supported-only partial writes, mixed MIME readback,
Added/Removed Association candidate eligibility, and catalog replacement
after an application is removed; `qindaqt.settings-default-apps-mime-types`
covers the per-type reads and writes File Manager's Open With uses. The
offscreen page gate verifies eight
categories, mixed selections, partial-scope labels, and keyboard choice
dispatch. Session tests prove login preserves existing user MIME files and
creates no new user MIME policy.

The packaged-defaults test stages the actual session install component, then
queries every supported core MIME type with `xdg-mime` inside temporary XDG
roots. It also covers optional browser/mail candidates, missing handlers,
user/admin overrides, desktop-specific precedence, and QindaQt-only scoping.
Its controlled desktop-entry fixtures are never launched. These gates do not
claim a live file-open launch through a deployed desktop session. The separate
installed-route test requires a complete Settings build with every route plugin.
