// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_windows.h"
#include "greeter_log.h"
#include "touch_keyboard.h"
#include <QLibraryInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QtGui/qscreen_platform.h>
#include <qpa/qplatformnativeinterface.h>
namespace QindaQt::LockGreeter {
namespace {
OutputKey keyOf(const QScreen *screen) { return screen; }
} // namespace
NativeWindows::NativeWindows(QGuiApplication &app, LockWorkerClient::WorkerProcess &worker,
                             SaverPresentation &saver, QString user)
    : m_app(app), m_worker(worker), m_saver(saver), m_user(std::move(user)),
      m_input(new InputActivityFilter(this)), m_runner(std::make_unique<WorkerAttemptRunner>(worker)),
      m_supervisor(*this, *this) {}
NativeWindows::~NativeWindows() {
  // Views first: they hold context properties pointing at the controller.
  qDeleteAll(m_windows);
  m_windows.clear();
}
bool NativeWindows::start() {
  m_app.setQuitOnLastWindowClosed(false);
  connect(&m_app, &QGuiApplication::screenAdded, this, &NativeWindows::track);
  connect(&m_app, &QGuiApplication::screenRemoved, this, &NativeWindows::untrack);
  for (auto *screen : m_app.screens()) track(screen);
  if (m_supervisor.windowCount() == 0) {
    qCWarning(lcGreeter, "no output has a lock window yet; waiting for outputs and retrying "
                         "(the compositor keeps the screen covered)");
  }
  return true;
}
void NativeWindows::track(QScreen *screen) {
  m_screens.insert(keyOf(screen), screen);
  m_supervisor.outputAdded(keyOf(screen));
}
void NativeWindows::untrack(QScreen *screen) {
  m_supervisor.outputRemoved(keyOf(screen));
  delete m_timers.take(keyOf(screen));
  m_screens.remove(keyOf(screen));
}
void NativeWindows::schedule(OutputKey output, std::chrono::milliseconds delay) {
  qCWarning(lcGreeter, "retrying the lock window of an output in %lld ms", static_cast<long long>(delay.count()));
  auto *&timer = m_timers[output];
  if (!timer) {
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, output] { m_supervisor.retryDue(output); });
  }
  timer->start(delay);
}
void NativeWindows::cancel(OutputKey output) { delete m_timers.take(output); }
void NativeWindows::destroy(OutputKey output) { delete m_windows.take(output); }
void NativeWindows::ensureController(LockProtocol::ProtocolClient &client) {
  if (m_controller) return;
  m_session = std::make_unique<ProtocolSession>(client);
  m_controller = std::make_unique<AuthenticationController>(*m_session, *m_runner, m_user);
  connect(m_controller.get(), &AuthenticationController::finished, &m_app, [] {
    qCInfo(lcGreeter, "the lock is finished; leaving the event loop");
    QCoreApplication::quit();
  });
  connect(m_input, &InputActivityFilter::activity, m_controller.get(), &AuthenticationController::noteInput);
}
OutputWindowFactory::Result NativeWindows::create(OutputKey output) {
  QScreen *screen = m_screens.value(output).data();
  if (!screen) return Result::NotAnOutput;
  const auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
  if (!native || !native->output()) return Result::NotAnOutput; // Qt's client-only placeholder is not an output.
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
  auto *native_interface = QGuiApplication::platformNativeInterface();
  // Observe the lock first, whether or not this output's role is granted, so a
  // lock the compositor already finished still ends the locker (see
  // LockProtocol::nativeObserverResource).
  if (auto *observer = static_cast<LockProtocol::ProtocolClient *>(
          native_interface->nativeResourceForWindow(LockProtocol::nativeObserverResource, view)))
    ensureController(*observer);
  auto *client = static_cast<LockProtocol::ProtocolClient *>(
      native_interface->nativeResourceForWindow(LockProtocol::nativeResource, view));
  if (!client) {
    qCWarning(lcGreeter, "the lock role for output '%s' was refused (lock already finished, a role for this "
                         "output exists, or no native window); the output stays covered by the compositor",
              qPrintable(screen->name()));
    delete view;
    return Result::Failed;
  }
  ensureController(*client);
  auto *keyboard = new TouchKeyboard(*client, view);
  auto *context = view->rootContext();
  context->setContextProperty(QStringLiteral("authentication"), m_controller.get());
  context->setContextProperty(QStringLiteral("keyboard"), &keyboard->model());
  context->setContextProperty(QStringLiteral("touchKeyboard"), keyboard);
  context->setContextProperty(QStringLiteral("saver"), &m_saver);
  view->setSource(QUrl(QStringLiteral("qrc:/native-lock/Main.qml")));
  if (view->status() == QQuickView::Error) {
    for (const auto &error : view->errors()) qCWarning(lcGreeter).noquote() << "greeter QML:" << error.toString();
    qCWarning(lcGreeter, "the greeter UI for output '%s' failed to load", qPrintable(screen->name()));
    delete view;
    return Result::Failed;
  }
  view->installEventFilter(m_input);
  m_windows.insert(output, view);
  view->show();
  return Result::Created;
}
} // namespace QindaQt::LockGreeter
