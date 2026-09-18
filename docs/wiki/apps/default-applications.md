# QindaQt Settings — Default applications route

`qindaqt-settings --page default-apps` lets the user choose the application
that opens for the web, mail, files, plain text, and image/video/music media,
per the freedesktop "Default Applications" mechanism. It composes only the
public installed-application catalog scanner and a direct local-file store;
it has no daemon, D-Bus authority, or subprocess.

## Mechanism: the same file xdg-mime and xdg-settings themselves use

`xdg-mime` and `xdg-settings` read and write exactly the `[Default
Applications]` group of the user's `$XDG_CONFIG_HOME/mimeapps.list`
(verified against real output on `qinda-top`: `xdg-mime query default
text/html` and `xdg-settings get default-web-browser` both echo that group's
entries exactly). This route reads and writes that same group directly
through KConfig — reparsed before every write, unrelated keys and groups
(including `[Added Associations]` and any mimetype this route does not
manage) left untouched — rather than shelling out to either tool. This
matches every other local-preference adapter in the codebase (PowerDevil
idle/lid, screen lock, autostart), which write their target file directly
for the same testability reason, and produces byte-identical externally
observable behavior to what the two tools would write.

An allow-list include scan rejects `QProcess` and any D-Bus include in this
route's files, enforcing that boundary structurally, not just by convention.

## Categories

Seven categories, each a fixed, documented mimetype set written together as
one unit (matching how `xdg-settings set default-web-browser` itself writes
`text/html` plus both HTTP(S) scheme handlers together):

| Category | Mimetypes written |
| --- | --- |
| Web browser | `text/html`, `x-scheme-handler/http`, `x-scheme-handler/https` |
| Mail client | `x-scheme-handler/mailto` |
| File manager | `inode/directory` |
| Text editor | `text/plain` |
| Image viewer | `image/jpeg`, `image/png`, `image/gif`, `image/webp` |
| Video player | `video/mp4`, `video/x-matroska`, `video/webm` |
| Music player | `audio/mpeg`, `audio/flac`, `audio/ogg` |

**Terminal is not listed.** Neither `xdg-mime` nor `xdg-settings` defines a
default-terminal association; there is no freedesktop-standard mimetype or
`xdg-settings` property for it. The page says so explicitly rather than
inventing a QindaQt-only mechanism for one category while every other
category follows the real standard. A QindaQt-specific terminal preference,
if wanted, is a separate follow-up with its own design, not a silent
addition here.

A category's displayed current default is read from its first
(representative) mimetype only, not merged across the set — a group another
tool has only partially written (one mimetype pointed at a different app
than the rest) is shown by that representative value alone rather than
averaged or guessed at.

## Candidate applications

Candidates come from `QindaQt::ApplicationCatalog::scanApplicationDirectories()`
(the same installed-`.desktop`-file scanner the file manager's "Open with"
picker uses, ADR-0164), resolved from `QStandardPaths::GenericDataLocation`
exactly as the file manager's `ApplicationsController` composition root
does. A desktop entry is offered for a category when its retained raw
`MimeType=` line (parsed from the `[Desktop Entry]` group only, matching the
Desktop Entry Specification's one-key-per-group rule) lists any one of that
category's mimetypes. The scan runs once at composition-root construction;
the route does not re-scan the filesystem on every page open.

## Composition and accessibility

The QML module owns a route-local `QML_SINGLETON` composition root
(`DefaultApplicationsRouteComposition`) owning one store and one already-
completed scan, mirroring `ClipboardRouteComposition`. Only the model QObject
is exposed to the page. Each category renders as one labeled `ComboBox` row
with a "None" option for an unset default; selecting an option writes
immediately (no separate apply step, since there is no daemon round-trip to
wait on). The `SettingsAppearanceRuntime` install component carries this
route's QML module alongside every other route's.

`Main.qml`'s per-route page `Component` definitions (this one included) live
in `SettingsRouteComponents.qml` rather than inline in `Main.qml`, which was
already at its file-lines review threshold before this route was added;
extracting them keeps `Main.qml` well under both the review threshold and
the hard file-lines ceiling instead of adding to a growing violation.

## Verification and stopping point

```sh
ctest --test-dir <build> -R '^qindaqt\.settings-default-apps-' --output-on-failure
```

Store round-trip (missing-file-is-empty, every category mimetype written
together, empty-id deletes rather than writes an empty value, unrelated
groups/keys preserved, representative-mimetype-only read), catalog parsing
(`[Desktop Entry]`-scoped `MimeType=` extraction, category filtering),
model (rows projection, persisted writes, unknown-category rejection, load-
failure/retry), an offscreen accessible page test (rendered rows, keyboard
selection dispatch), and the standard positive/hostile boundary scan. The
installed-route relocation row exists in the test matrix but was not run in
this candidate — it needs every other route's QML plugin built, which is the
integration gate's full-tree build, not a focused lane's.

Product and test runs never touch a host D-Bus session or system bus. This
slice does not claim a terminal default, a "browser also handles FTP/gemini"
scheme-handler sweep beyond the listed set, or live verification that a
real desktop launches the chosen application when a file is opened — that
end-to-end proof needs the built package deployed to a real session.
