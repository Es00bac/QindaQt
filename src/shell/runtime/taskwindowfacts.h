// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace QindaQt::ShellTaskList {
class TaskListSource;
}

namespace QindaQt::Shell {

// Application id and display title of a compositor window in the latest accepted task generation,
// container members included. False when the window is not in the list. The title is display text for
// the QindaLink focus relay (ADR-0440); never log it.
[[nodiscard]] bool taskWindowFacts(const ShellTaskList::TaskListSource &source, const QString &windowId,
                                   QString *applicationId, QString *title);

} // namespace QindaQt::Shell
