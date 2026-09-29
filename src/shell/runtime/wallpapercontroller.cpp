// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpapercontroller.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"
#include "shellpreferencevalues.h"
#include "shortcutnotecontroller.h"
#include "wallpaperselectionsource.h"
#include <LayerShellQt/Window>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QScreen>
#include <algorithm>
namespace QindaQt::Shell {
namespace {
// AGENT-NOTE: two stacked layers let a changed picture fade in over the old
// one (ADR-0286). `shownLayer` holds the settled picture; the other layer
// loads the next one and fades in above it only once it has an outcome, so a
// slow decode never flashes the fallback color. A layer is the fallback color
// plus its image, which makes "no wallpaper" fade like any other picture.
// The layers share one parent item so the shortcut-note card (ADR-0084),
// attached to the window later, always stays above both.
// AGENT-GUARD: url values are compared as strings; Qt 6 url wrappers are
// objects in JavaScript and `===` between two of them is identity.
constexpr auto WallpaperWindowQml = R"QML(import QtQuick
Window {
 id: root
 property url wallpaperSource
 property string wallpaperMode: "scaled"
 property int transitionMs: 0
 property int shownLayer: 0
 property bool presenting: false
 readonly property url presentedSource: root.shownLayer === 0 ? picture0.source : picture1.source
 readonly property int fill: root.wallpaperMode === "tiled" ? Image.Tile
   : root.wallpaperMode === "centered" ? Image.Pad : Image.PreserveAspectCrop
 color: "#172528"
 function layerAt(index) { return index === 0 ? layer0 : layer1 }
 function pictureAt(index) { return index === 0 ? picture0 : picture1 }
 function present() {
  if (!root.presenting) return
  fade.stop()
  const next = 1 - root.shownLayer
  root.layerAt(next).opacity = 0
  root.pictureAt(next).source = String(root.pictureAt(root.shownLayer).source)
      === String(root.wallpaperSource) ? "" : root.wallpaperSource
  root.reveal()
 }
 function reveal() {
  const next = 1 - root.shownLayer
  const picture = root.pictureAt(next)
  if (!root.presenting || fade.running || picture.status === Image.Loading
      || String(picture.source) !== String(root.wallpaperSource)
      || String(root.pictureAt(root.shownLayer).source) === String(root.wallpaperSource))
   return
  if (root.transitionMs <= 0) { root.settle(); return }
  fade.target = root.layerAt(next)
  fade.duration = root.transitionMs
  fade.start()
 }
 function settle() {
  const previous = root.shownLayer
  root.layerAt(1 - previous).opacity = 1
  root.shownLayer = 1 - previous
  root.layerAt(previous).opacity = 0
  root.pictureAt(previous).source = ""
 }
 onWallpaperSourceChanged: root.present()
 Component.onCompleted: {
  picture0.source = root.wallpaperSource
  root.presenting = true
 }
 Item {
  anchors.fill: parent
  Rectangle { id: layer0; anchors.fill: parent; color: root.color; z: root.shownLayer === 0 ? 0 : 1
   Image { id: picture0; objectName: "wallpaperPicture0"; anchors.fill: parent; asynchronous: true
    fillMode: root.fill; onStatusChanged: root.reveal() } }
  Rectangle { id: layer1; anchors.fill: parent; color: root.color; opacity: 0; z: root.shownLayer === 1 ? 0 : 1
   Image { id: picture1; objectName: "wallpaperPicture1"; anchors.fill: parent; asynchronous: true
    fillMode: root.fill; onStatusChanged: root.reveal() } }
 }
 NumberAnimation { id: fade; property: "opacity"; from: 0; to: 1; easing.type: Easing.InOutQuad
  onFinished: root.settle() }
})QML";
} // namespace
WallpaperController::WallpaperController(
    QGuiApplication &app, QQmlEngine &engine,
    Services::SettingsClient::SettingsClient &settings, QStringList dataRoots,
    QObject *parent)
    : QObject(parent), m_app(app), m_engine(engine), m_settings(settings),
      m_dataRoots(std::move(dataRoots)) {}
WallpaperController::~WallpaperController() { qDeleteAll(m_windows); }
void WallpaperController::start() {
  connect(&m_settings,
          &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &WallpaperController::applySnapshot);
  connect(&m_app, &QGuiApplication::screenAdded, this,
          [this](QScreen *) { reconcile(); });
  connect(&m_app, &QGuiApplication::screenRemoved, this,
          [this](QScreen *) { reconcile(); });
  applySnapshot();
  reconcile();
}
void WallpaperController::setSelectionSource(WallpaperSelectionSource *source) {
  if (m_selection == source)
    return;
  if (m_selection)
    disconnect(m_selection.data(), nullptr, this, nullptr);
  m_selection = source;
  if (m_selection)
    connect(m_selection.data(), &WallpaperSelectionSource::changed, this,
            &WallpaperController::refresh);
  refresh();
}
void WallpaperController::setMotionDuration(int milliseconds) {
  m_motionMilliseconds = std::clamp(milliseconds, 0, 1'000);
}
void WallpaperController::applySnapshot() {
  const auto &snapshot = m_settings.snapshot();
  if (!snapshot)
    return;
  const QVariantMap values = snapshot->values;
  if (values.value(QStringLiteral("appearance.wallpaper")).metaType().id() !=
          QMetaType::QString ||
      values.value(QStringLiteral("appearance.wallpaperMode"))
              .metaType()
              .id() != QMetaType::QString)
    return;
  m_everywhere = values.value(QStringLiteral("appearance.wallpaper")).toString();
  m_mode = values.value(QStringLiteral("appearance.wallpaperMode")).toString();
  const QVariant reducedMotion =
      values.value(QStringLiteral("accessibility.reducedMotion"));
  if (reducedMotion.metaType().id() == QMetaType::Bool)
    m_reducedMotion = reducedMotion.toBool();
  refresh();
}
int WallpaperController::transitionMilliseconds() const noexcept {
  return m_reducedMotion ? 0 : m_motionMilliseconds;
}
QUrl WallpaperController::sourceFor(const QScreen &screen) const {
  QString display;
  QString desktop;
  Services::WallpaperAssignments::WallpaperAssignments none;
  const Services::WallpaperAssignments::WallpaperAssignments *assignments = &none;
  if (m_selection) {
    display = m_selection->displayIdForConnector(screen.name());
    desktop = m_selection->currentDesktopId();
    assignments = &m_selection->assignments();
  }
  const QString path = resolveWallpaperSource(
      assignments->resolve(m_everywhere, display, desktop).wallpaper, m_dataRoots);
  return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}
void WallpaperController::refresh() {
  // AGENT-GUARD: transitionMs is written before wallpaperSource. The QML
  // change handler starts the fade and reads the duration at that moment.
  for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
    QQuickWindow *window = it.value();
    window->setProperty("transitionMs", transitionMilliseconds());
    window->setProperty("wallpaperMode", m_mode);
    window->setProperty("wallpaperSource", sourceFor(*it.key()));
  }
}
void WallpaperController::reconcile() {
  const auto screens = m_app.screens();
  for (auto it = m_windows.begin(); it != m_windows.end();) {
    if (!screens.contains(it.key())) {
      delete it.value();
      it = m_windows.erase(it);
    } else {
      ++it;
    }
  }
  for (QScreen *screen : screens)
    if (!m_windows.contains(screen))
      createWindow(screen);
}
void WallpaperController::setShortcutNote(ShortcutNoteController *note) {
  m_shortcutNote = note;
}
void WallpaperController::createWindow(QScreen *screen) {
  QQmlComponent component(&m_engine);
  component.setData(WallpaperWindowQml,
                    QUrl(QStringLiteral("qrc:/qindaqt/Wallpaper.qml")));
  // A new output paints its first picture directly; only later changes fade.
  QObject *object = component.createWithInitialProperties(
      {{QStringLiteral("wallpaperSource"), sourceFor(*screen)},
       {QStringLiteral("wallpaperMode"), m_mode},
       {QStringLiteral("transitionMs"), transitionMilliseconds()}});
  auto *raw = qobject_cast<QQuickWindow *>(object);
  if (!raw) {
    qWarning().noquote()
        << "QindaQt shell wallpaper component did not create a window:"
        << component.errorString();
    delete object;
    return;
  }
  raw->setScreen(screen);
  raw->setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
  auto *layer = LayerShellQt::Window::get(raw);
  if (!layer) {
    delete raw;
    return;
  }
  layer->setWantsToBeOnActiveScreen(false);
  layer->setScreen(screen);
  // AGENT-CONTRACT: KWin LayerShellV1Window maps the exact `desktop` scope to
  // WindowType::Desktop. Any other scope becomes Normal and contaminates
  // task-list, active-window, and shell-visibility facts.
  layer->setScope(QStringLiteral("desktop"));
  LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
  anchors |= LayerShellQt::Window::AnchorBottom;
  anchors |= LayerShellQt::Window::AnchorLeft;
  anchors |= LayerShellQt::Window::AnchorRight;
  layer->setAnchors(anchors);
  layer->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
  layer->setLayer(LayerShellQt::Window::LayerBackground);
  layer->setExclusiveZone(-1);
  layer->setCloseOnDismissed(false);
  layer->setDesiredSize(QSize(0, 0));
  raw->show();
  m_windows.insert(screen, raw);
  if (m_shortcutNote) {
    m_shortcutNote->attachToWindow(*raw, screen->name());
  }
}
} // namespace QindaQt::Shell
