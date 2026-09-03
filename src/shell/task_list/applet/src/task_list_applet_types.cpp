// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_types.h"

namespace QindaQt::ShellTaskListApplet {

QString taskListAppletPhaseText(TaskListAppletPhase phase) {
  switch (phase) {
  case TaskListAppletPhase::Loading:
    return QStringLiteral("loading");
  case TaskListAppletPhase::Ready:
    return QStringLiteral("ready");
  case TaskListAppletPhase::Empty:
    return QStringLiteral("empty");
  case TaskListAppletPhase::Degraded:
    return QStringLiteral("degraded");
  case TaskListAppletPhase::Unavailable:
    return QStringLiteral("unavailable");
  }
  Q_UNREACHABLE_RETURN(QStringLiteral("unavailable"));
}

} // namespace QindaQt::ShellTaskListApplet
