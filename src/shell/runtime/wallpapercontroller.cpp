// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpapercontroller.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "shellpreferencevalues.h"
#include "shortcutnotecontroller.h"
#include <LayerShellQt/Window>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QScreen>
namespace QindaQt::Shell {
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
  m_source = resolveWallpaperSource(
      values.value(QStringLiteral("appearance.wallpaper")).toString(),
      m_dataRoots);
  m_mode = values.value(QStringLiteral("appearance.wallpaperMode")).toString();
  for (QQuickWindow *window : m_windows) {
    window->setProperty("wallpaperSource", QUrl::fromLocalFile(m_source));
    window->setProperty("wallpaperMode", m_mode);
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
  component.setData(R"QML(import QtQuick
Window {
 id: root
 property url wallpaperSource
 property string wallpaperMode: "scaled"
 color: "#172528"
 Image { anchors.fill: parent; source: root.wallpaperSource; asynchronous: true
  fillMode: root.wallpaperMode === "tiled" ? Image.Tile
    : root.wallpaperMode === "centered" ? Image.Pad : Image.PreserveAspectCrop }
})QML",
                    QUrl(QStringLiteral("qrc:/qindaqt/Wallpaper.qml")));
  QObject *object = component.createWithInitialProperties(
      {{QStringLiteral("wallpaperSource"), QUrl::fromLocalFile(m_source)},
       {QStringLiteral("wallpaperMode"), m_mode}});
  auto *raw = qobject_cast<QQuickWindow *>(object);
  if (!raw) {
    qWarning().noquote()
        << "QindaQt shell wallpaper component did not create a window";
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
