// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskwindowfacts.h"

#include "qindaqt/shell/task_list/task_list_source.h"

namespace QindaQt::Shell {

bool taskWindowFacts(const ShellTaskList::TaskListSource &source, const QString &windowId,
                     QString *applicationId, QString *title)
{
    for (const ShellTaskList::TaskEntry &entry : source.generation().entries) {
        if (entry.members.isEmpty()) {
            if (entry.taskId == windowId) {
                *applicationId = entry.applicationId;
                *title = entry.title;
                return true;
            }
            continue;
        }
        for (const ShellTaskList::TaskContainerMember &member : entry.members) {
            if (member.windowId == windowId) {
                *applicationId = member.applicationId;
                *title = member.title;
                return true;
            }
        }
    }
    return false;
}

} // namespace QindaQt::Shell
