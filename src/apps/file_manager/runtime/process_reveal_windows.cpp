// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_reveal_windows.h"

#include "../model/navigation_controller.h"
#include "../public/desktop_file_boundary.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVariant>
#include <QWindow>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

// AGENT-NOTE: a FileManager1 StartupId is the caller's activation token. Qt's
// Wayland plugin activates a window with XDG_ACTIVATION_TOKEN, read (and
// cleared) when a window is first shown or asks for activation; its X11
// plugin reads DESKTOP_STARTUP_ID at start-up only.
[[nodiscard]] bool onWayland() {
  return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

bool startDetached(const QString &program, const QStringList &arguments,
                   const QString &activationToken) {
  QProcess process;
  process.setProgram(program);
  process.setArguments(arguments);
  if (!activationToken.isEmpty()) {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(onWayland() ? QStringLiteral("XDG_ACTIVATION_TOKEN")
                                   : QStringLiteral("DESKTOP_STARTUP_ID"),
                       activationToken);
    process.setProcessEnvironment(environment);
  }
  return process.startDetached();
}

} // namespace

ProcessRevealWindows::ProcessRevealWindows(QWindow &window, NavigationController &navigation,
                                           QString program, RevealProcessStarter start)
    : m_window(window), m_navigation(navigation), m_program(std::move(program)),
      m_start(start ? std::move(start) : RevealProcessStarter(startDetached)) {}

bool ProcessRevealWindows::show(const RevealRequest &request, const QString &activationToken) {
  // A hidden window is a --service start that has shown nothing yet: after a
  // window is shown, closing it ends the process.
  if (!m_window.isVisible() || m_navigation.currentPath() == request.folder) {
    return showHere(request, activationToken);
  }
  return m_start(m_program, Desktop::FileBoundary::revealArguments(request), activationToken);
}

bool ProcessRevealWindows::showHere(const RevealRequest &request,
                                    const QString &activationToken) {
  // AGENT-CONTRACT: ui/EntryReveal.qml is found by its objectName and answers
  // reveal(folder, names, action) with QVariant arguments.
  QObject *reveal = m_window.findChild<QObject *>(QStringLiteral("entryReveal"));
  const QVariant folder(request.folder);
  const QVariant names(request.names);
  const QVariant action(request.action);
  QVariant shown;
  if (reveal == nullptr ||
      !QMetaObject::invokeMethod(reveal, "reveal", Q_RETURN_ARG(QVariant, shown),
                                 Q_ARG(QVariant, folder), Q_ARG(QVariant, names),
                                 Q_ARG(QVariant, action))) {
    return false;
  }
  // The token is consumed here and never left in the environment, where
  // processes this window starts later would inherit it.
  const bool token = !activationToken.isEmpty() && onWayland();
  if (token) {
    qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
  }
  // No raise(): a Wayland client cannot raise itself (see WindowServices.qml);
  // show() plus requestActivate() is what the compositor acts on.
  m_window.show();
  m_window.requestActivate();
  if (token) {
    qunsetenv("XDG_ACTIVATION_TOKEN");
  }
  return shown.toBool();
}

} // namespace QindaQt::Apps::FileManager
