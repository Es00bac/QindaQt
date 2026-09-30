// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_windows.h"
#include "touch_keyboard.h"
#include <QLibraryInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QtGui/qscreen_platform.h>
#include <qpa/qplatformnativeinterface.h>
namespace QindaQt::LockGreeter {
NativeWindows::NativeWindows(QGuiApplication &app, LockWorkerClient::WorkerProcess &worker,
                             SaverPresentation &saver, QString user)
    : m_app(app), m_worker(worker), m_saver(saver), m_user(std::move(user)) {}
NativeWindows::~NativeWindows() { qDeleteAll(m_windows); }
bool NativeWindows::start() {
  m_app.setQuitOnLastWindowClosed(false);
  for (auto *screen : m_app.screens()) if (!create(screen)) return false;
  connect(&m_app, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
    if (!create(screen)) QCoreApplication::exit(2); // Server remains fail-closed black.
  });
  connect(&m_app, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
    delete m_windows.take(screen);
  });
  return !m_windows.isEmpty();
}
bool NativeWindows::create(QScreen *screen) {
  const auto *output = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
  if (!output || !output->output()) return true; // Qt's client-only placeholder is not an output.
  auto *view = new QQuickView;
  view->engine()->setImportPathList({QStringLiteral("qrc:/qt/qml"),
                                    QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
#if defined(QINDAQT_PRIVATE_REEF_QML_ROOT)
  // Compile-time only, non-installed fixture uses the exact uninstalled Reef
  // prerequisite. Production never accepts an alternate import root.
  view->engine()->addImportPath(QStringLiteral(QINDAQT_PRIVATE_REEF_QML_ROOT));
#endif
  view->setScreen(screen); view->setFlags(Qt::Window | Qt::FramelessWindowHint);
  view->setGeometry(screen->geometry()); view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setColor(QColor(QStringLiteral("#08111e"))); view->create();
  auto *client = static_cast<LockProtocol::ProtocolClient *>(
      QGuiApplication::platformNativeInterface()->nativeResourceForWindow(LockProtocol::nativeResource, view));
  if (!client) { delete view; return false; }
  if (!m_controller) {
    m_controller = std::make_unique<AuthenticationController>(*client, m_worker, m_user);
    connect(m_controller.get(), &AuthenticationController::finished, &m_app, [] { QCoreApplication::quit(); });
  }
  auto *keyboard = new TouchKeyboard(*client, view);
  auto *context = view->rootContext();
  context->setContextProperty(QStringLiteral("authentication"), m_controller.get());
  context->setContextProperty(QStringLiteral("keyboard"), &keyboard->model());
  context->setContextProperty(QStringLiteral("touchKeyboard"), keyboard);
  context->setContextProperty(QStringLiteral("saver"), &m_saver);
  view->setSource(QUrl(QStringLiteral("qrc:/native-lock/Main.qml")));
  if (view->status() == QQuickView::Error) { delete view; return false; }
  m_windows.insert(screen, view); view->show(); return true;
}
}
