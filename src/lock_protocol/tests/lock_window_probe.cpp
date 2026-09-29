// SPDX-License-Identifier: GPL-3.0-or-later
// Non-installed Qt native-role interoperability fixture, no PAM/unlock API.
#include "protocol_client.h"
#include <QBackingStore>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QHash>
#include <QLibraryInfo>
#include <QPainter>
#include <QScreen>
#include <QtGui/qscreen_platform.h>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <cstdio>
#include <sys/prctl.h>
#include <sys/resource.h>
using namespace QindaQt::LockProtocol;
class ProbeWindow final : public QWindow {
public:
  ProbeWindow() : m_store(this) {}
protected:
  void exposeEvent(QExposeEvent *) override { render(); }
  void resizeEvent(QResizeEvent *) override { render(); }
private:
  void render() {
    if (!isExposed() || size().isEmpty()) return;
    m_store.resize(size()); m_store.beginPaint(QRegion(QRect(QPoint(), size())));
    QPainter painter(m_store.paintDevice()); painter.fillRect(QRect(QPoint(), size()), QColor("#345678"));
    painter.end(); m_store.endPaint(); m_store.flush(QRegion(QRect(QPoint(), size())));
    if (!m_painted) { m_painted = true; std::puts("NATIVE_ROLE_PAINTED"); std::fflush(stdout); }
  }
  QBackingStore m_store;
  bool m_painted = false;
};
int main(int argc, char **argv) {
  const rlimit noCore{0, 0};
  if (setrlimit(RLIMIT_CORE, &noCore) || prctl(PR_SET_DUMPABLE, 0) || prctl(PR_GET_DUMPABLE)) return 2;
  // Compile-time private fixture path only, never a production import option.
  QCoreApplication::setLibraryPaths({QStringLiteral(QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT),
                                    QLibraryInfo::path(QLibraryInfo::PluginsPath)});
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "qindaqt-session-lock");
  qputenv("QT_FATAL_WARNINGS", "1");
  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  if (app.platformName() != QStringLiteral("wayland")) return 2;
  QHash<QScreen *, ProbeWindow *> windows;
  ProtocolClient *client = nullptr;
  auto create = [&](QScreen *screen) {
    const auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
    if (!native || !native->output()) return true; // Qt placeholder has no physical role.
    auto *window = new ProbeWindow;
    window->setScreen(screen); window->setFlags(Qt::Window | Qt::FramelessWindowHint);
    window->setGeometry(screen->geometry()); window->create();
    auto *role = static_cast<ProtocolClient *>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeResource, window));
    if (!role) { delete window; std::puts("NATIVE_ROLE_UNAVAILABLE"); std::fflush(stdout); return false; }
    if (!client) {
      client = role;
      QObject::connect(client, &ProtocolClient::locked, &app, [] { std::puts("NATIVE_PROTOCOL_LOCKED"); std::fflush(stdout); });
      QObject::connect(client, &ProtocolClient::rejected, &app, [] { QCoreApplication::exit(2); });
    }
    if (client != role) { delete window; return false; }
    windows.insert(screen, window); window->show(); return true;
  };
  for (auto *screen : app.screens()) if (!create(screen)) return 2;
  QObject::connect(&app, &QGuiApplication::screenAdded, &app, [&](QScreen *screen) {
    if (!create(screen)) QCoreApplication::exit(2);
  });
  QObject::connect(&app, &QGuiApplication::screenRemoved, &app, [&](QScreen *screen) {
    delete windows.take(screen);
  });
  const int result = app.exec(); qDeleteAll(windows); return result;
}
