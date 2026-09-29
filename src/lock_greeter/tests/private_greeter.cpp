// SPDX-License-Identifier: GPL-3.0-or-later
// Non-installed real Qt role + real private-confdir PAM conversation fixture.
// Input commands are synthetic UI intent, never an approval/result injection.
#include "native_windows.h"
#include "process_protection.h"
#include "touch_keyboard.h"
#include <QQuickItem>
#include <QTimer>
#include <QFile>
#include <QLibraryInfo>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <cstdio>
#include <unistd.h>
using namespace QindaQt;
class BlankPreferences final : public Session::DesktopControls::ScreensaverPreferencesProvider {
public:
  Session::DesktopControls::ScreensaverPreferences currentPreferences() const override { return m_current; }
  void refresh() override {}
  void select(QString token) { m_current.saver = std::move(token); Q_EMIT preferencesChanged(m_current); }
private:
  Session::DesktopControls::ScreensaverPreferences m_current;
};
int main(int argc, char **argv) {
  if (argc != 1 || !LockPlatform::protectAuthority() || !LockPlatform::restrictedPtracePolicy()) return 2;
  QCoreApplication::setLibraryPaths({QStringLiteral(QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT),
                                    QLibraryInfo::path(QLibraryInfo::PluginsPath)});
  qputenv("QT_QPA_PLATFORM", "wayland");
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "qindaqt-session-lock");
  qputenv("QT_FATAL_WARNINGS", "1");
  QGuiApplication app(argc, argv);
  if (app.platformName() != QStringLiteral("wayland")) return 2;
  QTemporaryDir directory;
  if (!directory.isValid()) return 2;
  auto configure = [&](QByteArray account, QByteArray auth = "permit") {
    QFile file(directory.filePath(QStringLiteral("qindaqt-lock")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray module = QINDAQT_PRIVATE_PAM_MODULE_PATH;
    const auto data = "auth required " + module + " " + auth + "\naccount required " + module + " " + account +
                      "\nsession optional " + module + " permit\n";
    return file.write(data) == data.size();
  };
  if (!configure("permit")) return 2;
  LockWorkerClient::WorkerProcess worker(QStringLiteral(QINDAQT_PRIVATE_WORKER_PATH), directory.path(), 3000);
  BlankPreferences preferences;
  LockGreeter::SaverPresentation saver(preferences);
  LockGreeter::NativeWindows windows(app, worker, saver, QStringLiteral("Synthetic fixture user"));
  if (!windows.start()) return 2;
  auto *controller = windows.controller(); if (!controller) return 2;
  QObject::connect(&worker, &LockWorkerClient::WorkerProcess::prompt, &app,
      [&](LockAuthentication::MessageKind kind, const QString &text) {
    if (kind == LockAuthentication::MessageKind::Information &&
        text == QStringLiteral("Synthetic fixture entering blocked PAM module")) {
      std::printf("NATIVE_WORKER_BLOCKED=%lld\n", static_cast<long long>(worker.testProcessId()));
      std::fflush(stdout);
    }
  });
  bool promptReported = false;
  QObject::connect(controller, &LockGreeter::AuthenticationController::changed, &app, [&] {
    if (controller->waiting() && !promptReported) {
      promptReported = true; std::puts("NATIVE_GREETER_PROMPT"); std::fflush(stdout);
    }
    if (!controller->waiting()) promptReported = false;
    if (!controller->busy()) { std::puts("NATIVE_GREETER_IDLE"); std::fflush(stdout); }
    if (!controller->busy() && !controller->status().isEmpty()) {
      std::puts("NATIVE_GREETER_DENIED"); std::fflush(stdout);
    }
  });
  QObject::connect(controller, &LockGreeter::AuthenticationController::finished, &app, [] {
    std::puts("NATIVE_GREETER_FINISHED"); std::fflush(stdout);
  });
  for (auto *window : app.topLevelWindows()) {
    auto *view = qobject_cast<QQuickView *>(window);
    if (!view) continue;
    QObject::connect(view, &QQuickWindow::frameSwapped, &app, [view, controller] {
      auto *field = view->rootObject() ? view->rootObject()->findChild<QObject *>("nativeLockPassword") : nullptr;
      if (controller->waiting() && field && field->property("visible").toBool()) {
        std::printf("NATIVE_GREETER_UI_READY DPR=%.2f\n", static_cast<double>(view->devicePixelRatio())); std::fflush(stdout);
      }
    });
  }
  QTimer scenePoll;
  scenePoll.setInterval(20);
  QObject::connect(&scenePoll, &QTimer::timeout, &app, [&] {
    for (auto *window : app.topLevelWindows()) {
      auto *view = qobject_cast<QQuickView *>(window);
      if (!view || !view->rootObject() || !view->rootObject()->property("saverReady").toBool()) return;
    }
    scenePoll.stop(); std::puts("NATIVE_SAVER_READY"); std::fflush(stdout);
  });
  auto touchPassword = [&] {
    auto *view = qobject_cast<QQuickView *>(app.topLevelWindows().first());
    auto *keyboard = view ? view->findChild<LockGreeter::TouchKeyboard *>() : nullptr;
    if (!keyboard || !view->rootObject()) return false;
    view->rootObject()->setProperty("keyboardVisible", true);
    auto &model = keyboard->model();
    for (QChar character : QStringLiteral("fixture-response")) {
      bool found = false;
      for (bool symbols : {false, true}) {
        model.showSymbols(symbols);
        const auto &rows = symbols ? model.document().symbols : model.document().letters;
        for (int r = 0; r < rows.size() && !found; ++r)
          for (int k = 0; k < rows[r].size(); ++k)
            if (rows[r][k].text == QString(character)) { model.press(r, k); found = true; break; }
        if (found) break;
      }
      if (!found) return false;
    }
    // Exercise the actual QML credential sink and its submit handler.
    return QMetaObject::invokeMethod(view->rootObject(), "submit");
  };
  QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
  QByteArray pending;
  QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
    char bytes[128]; const auto count = read(STDIN_FILENO, bytes, sizeof(bytes));
    if (count <= 0) { input.setEnabled(false); return; }
    pending.append(bytes, count);
    if (pending.size() > 4096) { QCoreApplication::exit(2); return; }
    for (;;) {
      const auto end = pending.indexOf('\n'); if (end < 0) break;
      const auto command = pending.left(end); pending.remove(0, end + 1);
      if (command == "bad-password") controller->respond(QStringLiteral("wrong-synthetic-password"));
      else if (command == "password") controller->respond(QStringLiteral("fixture-response"));
      else if (command == "touch-password") { if (!touchPassword()) QCoreApplication::exit(2); }
      else if (command == "saver-patrol") { preferences.select(QStringLiteral("qinda-patrol")); scenePoll.start(); }
      else if (command == "saver-reef") { preferences.select(QStringLiteral("circuit-reef")); scenePoll.start(); }
      else if (command == "cancel") { controller->cancel(); std::puts("NATIVE_GREETER_CANCELLED"); std::fflush(stdout); }
      else if (command == "begin") controller->begin();
      else if (command == "block-worker") { controller->cancel(); if (!worker.active() && configure("permit", "block")) controller->begin(); }
      else if (command == "account-deny") { if (!worker.active() && configure("account-deny")) controller->begin(); }
      else if (command == "account-permit") { if (!worker.active() && configure("permit")) controller->begin(); }
      else QCoreApplication::exit(2);
    }
  });
  return app.exec();
}
