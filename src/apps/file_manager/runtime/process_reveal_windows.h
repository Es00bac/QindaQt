// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager1_service.h"

#include <QString>
#include <QStringList>

#include <functional>

class QWindow;

namespace QindaQt::Apps::FileManager {

class NavigationController;

// Starts `program` detached with literal `arguments`, handing the child
// `activationToken` (possibly empty) so its window may take focus.
using RevealProcessStarter = std::function<bool(
    const QString &program, const QStringList &arguments, const QString &activationToken)>;

// Production RevealWindows (ADR-0273). A File Manager process owns exactly one
// window, so a request is shown here only while that window has shown nothing
// yet (a --service start for D-Bus activation) or already shows the folder.
// Any other request starts one more File Manager process with
// Desktop::FileBoundary::revealArguments(), the way the desktop opens a
// folder: no shell, and the new process revalidates its folder argument.
class ProcessRevealWindows final : public RevealWindows {
public:
  // `window` is this process's File Manager window, the QML root carrying the
  // "entryReveal" object (ui/EntryReveal.qml); `navigation` is its controller.
  // Both must outlive this object. `program` is started for a new window; an
  // empty `start` means QProcess::startDetached with the token in the child's
  // environment.
  ProcessRevealWindows(QWindow &window, NavigationController &navigation, QString program,
                       RevealProcessStarter start = {});

  [[nodiscard]] bool show(const RevealRequest &request,
                          const QString &activationToken) override;
  // Shows the request in this window, whatever it shows now.
  [[nodiscard]] bool showHere(const RevealRequest &request, const QString &activationToken);

private:
  QWindow &m_window;
  NavigationController &m_navigation;
  QString m_program;
  RevealProcessStarter m_start;
};

} // namespace QindaQt::Apps::FileManager
