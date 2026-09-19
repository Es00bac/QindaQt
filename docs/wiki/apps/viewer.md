# QindaQt Viewer

QindaQt Viewer (`qindaqt-viewer`, `org.qindaqt.Viewer.desktop`) opens local images
and PDFs in an ordinary application window. It is C++20 and GPL-3.0-or-later.
Its window, menus, toolbar, password dialog, status bar and scrollable/zoomable
viewport use **QindaTK**. PDF parsing and rasterization use the existing
[Poppler Qt6 library](https://poppler.freedesktop.org/api/qt6/); image decoding
uses [Qt's image readers](https://doc.qt.io/qt-6/qimagereader.html).

The toolkit and dependency choice is recorded in
[ADR-0217](../adr/0217-use-qindatk-and-poppler-for-the-viewer.md). Public
[AppShell](application-shell.md) supplies action/menu projection and standard
desktop menu export. QindaTK supplies presentation; the viewer never reaches
into shell or toolkit private implementation.

## Opening and viewing

Use **Open**, Ctrl+O, one dropped local file, or a path/file URL on the command
line. The desktop entry uses `%f`; opening several files creates separate
application invocations through the desktop launcher. A direct command with
more than one positional file exits with an explanation. HTTP and other remote
URLs are rejected without a network request. The viewer does not write the
opened file, remember a history, or persist passwords.

The advertised formats are PDF, JPEG, PNG, GIF, WebP, BMP, TIFF and SVG. Qt's
imageformats plugins are required for WebP/TIFF and Qt SVG for SVG. Animated
images and multi-image TIFFs display their first frame; PDF page controls are
available for all pages. Image orientation metadata is respected. Password
protected PDFs prompt with a masked field; an incorrect password offers another
attempt and cancellation leaves a visible Unlock action.

| Command | Shortcut |
| --- | --- |
| Open / close document / quit | Ctrl+O / Ctrl+W / Ctrl+Q |
| Previous / next PDF page | Page Up / Page Down |
| First / last PDF page | Ctrl+Home / Ctrl+End |
| Zoom in / out | Ctrl++ or Ctrl+= / Ctrl+- |
| Actual size / fit page / fit width | Ctrl+0 / Ctrl+1 / Ctrl+2 |
| Rotate clockwise | Ctrl+R |
| Zoom about the cursor | Ctrl+wheel |
| Pan / horizontal scroll | Middle-button drag / Shift+wheel |

The page-number field also jumps directly to a PDF page. Fit modes update as
the window or page size changes; manual zoom exits fit mode. Zoom ranges from
5% to 800%, with render resolution bounded independently. PDF actual size uses
96 logical pixels per inch. Rotation changes only the view. The toolbar wraps
in narrow windows; QindaTK controls retain keyboard focus and accessible names.
Errors and empty/locked state are visible in the content area. Rendering runs
off the GUI thread and leaves a status message while work is pending.

The local File/View menu and standard exported menu share AppShell's action
snapshot and enabled state. The local menu remains visible unless the desktop
confirms that it hosts this exact menu endpoint. No private transport is added.

## Ownership and resource limits

`src/apps/viewer` owns the renderer, GUI-thread controller, frame image provider,
application composition and QML. `DocumentRenderer` owns one Poppler document
or decoded image on a serialized worker thread. `ViewerController` owns that
thread and joins it during teardown. Only value requests/results cross the
thread boundary; Poppler objects never enter QML or the scene graph. Each
request has a revision, and close/open/navigation/zoom invalidate old results.
Obsolete queued work is skipped, and Poppler's cancellation callback stops an
obsolete PDF render. Closing a document clears cached pixels and passwords.
Password strings cross the Poppler Qt6 boundary using its documented Latin-1
byte encoding, including non-ASCII legacy-PDF passwords.

Raster output is limited to **16 Mi pixels**, with an **8192-pixel maximum
edge**. Unsupported full-image scaling rejects source images above 64 Mi
pixels before decode; Qt's image allocation limit remains enabled at 256 MiB.
These bounds limit application-owned raster allocations, not all memory used
inside a PDF parser. A complex PDF may take time to parse; no process sandbox
or hard parser deadline is claimed. Passwords are not logged by application
code. The renderer does not execute PDF JavaScript, follow document links, or
launch applications.

## Build and verification

The app requires installed QindaTK, Poppler's `poppler-qt6` pkg-config module,
Qt Quick/Quick Controls/Dialogs/SVG and the Qt imageformats plugins. The viewer
build registers the `Viewer` install component, including its executable,
desktop entry/icon and public AppShell backing libraries. QindaTK and Qt/Poppler
remain normal system dependencies. The app's QML is embedded in the executable,
so it does not depend on source/build-tree import paths after installation.

```sh
cmake --build build/dev --target qindaqt-viewer qindaqt_viewer_renderer_test \
  qindaqt_viewer_controller_test qindaqt_viewer_ui_test -j2
ctest --test-dir build/dev -R '^apps.viewer\.' --output-on-failure
```

The four focused gates render genuine generated PDFs and every advertised
image format, check password failures/success, navigation/rotation/size bounds,
latest-open and close fencing, QindaTK UI keyboard paths and 960×680/640×480
captures, then exercise CLI paths/file URLs and a relocated `Viewer` install.
All run with isolated home/runtime roots and offscreen Qt. `--screenshot PATH`
is the verification seam: capture a settled window and exit, nonzero on an
open/capture error or a 30-second verification deadline. Host desktop settings
are never needed for these gates.

This is a read-only image/PDF viewer. Search, text selection/copy, annotation,
printing, PDF forms, image editing, animation and continuous-page layout are
not implemented. The offscreen checks do not claim physical-display or
assistive-technology qualification. QindaTK's existing theme is used directly;
there is no viewer-specific theme or token derivation.
