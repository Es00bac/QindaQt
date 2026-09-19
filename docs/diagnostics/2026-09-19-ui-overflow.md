# September 19 UI diagnostics

Source inspection precedes implementation and compilation for this repair.
No runtime tests were used to establish these findings. The initial desktop
source is `01e919f6bff7c3c111778f814aaec8288ddd7777` on both qinda and qinda-top.

## Confirmed causes and repair decisions

- **Global menu:** `6b6ed550` caps `GlobalMenuNativeMenu.height` but leaves
  `implicitHeight` equal to the entire menu. In the installed Qt 6.11.1 source,
  `QQuickPopupWindow::implicitHeightChanged()` sets both the native window and
  popup item's height to that uncapped implicit height. A late list layout or
  submenu publication can therefore undo the cap. The list then decides that
  it fits and stops accepting wheel scrolling. Cap the implicit size itself,
  retain the inherited screen budget for submenus, and compare content height
  with the actual list viewport. Preserve Qt's menu focus and activation.
- **Settings navigation:** `SettingsSidebar.qml` contains nested ColumnLayouts
  and a spacer, with no Flickable or ScrollView. The route list's minimum
  height exceeds a short window. Give the complete grouped route list a bounded
  viewport, a visible scrollbar when needed, and reveal a route on selection
  or keyboard focus without continuously forcing the scroll position.
  The compact header's horizontal Flickable also ignores an ordinary vertical
  mouse wheel; map wheel motion onto its one scroll axis and expose its bar.
- **File Manager navigation:** `PlacesSidebar.qml` scrolls only bookmarks.
  Fixed places and up to 64 saved network locations sit above that viewport
  in a ColumnLayout. Those rows can consume all available height. Use one
  viewport for Places, Network and Bookmarks, preserving their actions, drop
  targets and accessible identities. Reveal focused rows, including Remove.
- **Tray menus:** `StatusNotifierMenu.qml` also uses `Popup.Window`, but has no
  height cap. Apply the same implicit-height budget to every submenu.
- **Top-level menu overflow:** the global bar's `+N` indicator is only a Text
  item. Folded menus have no activation path even though local menus can be
  hidden while the shell hosts them. Make the existing indicator an accessible
  button opening the same native menu renderer over those omitted entries.
- **QindaTK menus:** `Menu.qml` compares content height with the enclosing
  window, rather than the menu viewport, and has no screen cap. Even an item
  popup that Qt shrinks near a window edge can consequently refuse the wheel.
  Cap implicit height and use the actual viewport. This shared repair reaches
  Office, Viewer, the media player and other toolkit consumers.
- **QindaTK dialogs:** `Dialog.qml` sizes itself to its full body with no
  height limit or body scroller. Long preferences/forms can hide their footer
  outside a short application window. Bound the dialog to its host and scroll
  its body while keeping the existing header/footer and content API.
- **Welcome and Customize:** Welcome's seven chapter buttons share Settings'
  unscrollable-column pattern; its headings, spacing and buttons exceed the
  supported 480-pixel height. Scroll the chapter column. Compact Customize
  nests its already-scrollable inspector inside another ScrollView; give that
  inspector the available viewport directly.
- **File Manager route switching:** Applications and Network are independent
  booleans; the stack gives Network priority. Opening Applications after
  Network can therefore leave Network displayed. Each route activation must
  clear the other flag.
- **File Manager preferences:** each preference page is a bare ColumnLayout.
  Network help and error banners can exceed the 320-pixel minimum window.
  Bound each page with its own ScrollView while retaining the common footer.

The Qt evidence was read from the installed version's source archive,
`qtdeclarative-everywhere-src-6.11.1.tar.xz`, specifically
`src/quicktemplates/qquickpopupwindow.cpp` (constructor, resize event and
implicit-height handler), `qquickpopup.cpp` (popup parenting) and the Basic
Menu's ListView. The public [Qt popup documentation](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html)
describes the separate-window path used here.

## Ecosystem and host inventory

qinda's source checkouts are under `~/work_SPaC3`; qinda-top's are under
`~/work_space`. Shared Git repositories already live on qinda under `~/git`,
and `qinda-sync code CHECKOUT` exchanges clean committed branches. Desktop,
toolkit, Office, Studio, player, Deck, Venus and overlay checkouts were
inventoried. The untracked QindaFox work in QindaQt_Apps is unrelated.

Existing CMake caches, active worktrees and sync paths depend on their current
physical locations. Group the ecosystem through a `~/QindaQt` directory of
links on each host rather than relocating active repositories during a UI fix.
Keep unrelated projects, historical backups and worker directories intact.

## Verification boundary

Finish all UI and separately assigned audio/OBS code before any compilation.
Build with the actual configured Portage job/load policy. Only afterward run
focused final checks of affected behavior; no new test campaign or baseline
test run. Record the final build, installed versions and remaining limitations
in the delivery handoff rather than treating this source diagnosis as proof
of live behavior.
