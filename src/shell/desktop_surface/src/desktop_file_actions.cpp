// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_file_actions.h"

#include "file_manager_menu_catalog.h"

namespace QindaQt::Shell::DesktopSurface {

DesktopFileActions::DesktopFileActions(QObject *parent) : QObject(parent) {}

QString DesktopFileActions::label(const QString &actionId) const
{
    const auto definition = QindaQt::Apps::FileManager::MenuCatalog::findAction(actionId);
    return definition ? definition->label : QString();
}

} // namespace QindaQt::Shell::DesktopSurface
