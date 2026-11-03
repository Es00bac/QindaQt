// SPDX-License-Identifier: GPL-3.0-or-later
#include "link_actions.h"

#include <QtCore/QCoreApplication>

namespace QindaQt::LinkApp {
namespace {

AppShell::ActionSpec action(const QString &id, const char *menu, const char *menuLabel, const char *label,
                            const char *shortcut, int menuOrder, int order)
{
    AppShell::ActionSpec spec;
    spec.id = id;
    spec.menuId = QString::fromLatin1(menu);
    spec.menuLabel = QCoreApplication::translate("LinkActions", menuLabel);
    spec.label = QCoreApplication::translate("LinkActions", label);
    spec.shortcut = QKeySequence(QString::fromLatin1(shortcut));
    spec.menuOrder = menuOrder;
    spec.order = order;
    return spec;
}

} // namespace

QList<AppShell::ActionSpec> linkActions()
{
    // AGENT-GUARD: AppShell's registry rejects an action with no shortcut ("invalid presentation
    // state"), which drops the whole catalog, so every entry below carries one.
    return {
        action(QStringLiteral("view.refresh"), "view", "View", "Refresh", "F5", 0, 0),
        action(QStringLiteral("go.devices"), "go", "Go", "Devices", "Ctrl+1", 1, 0),
        action(QStringLiteral("go.messages"), "go", "Go", "Messages", "Ctrl+2", 1, 1),
        action(QStringLiteral("go.apps"), "go", "Go", "Phone apps", "Ctrl+3", 1, 2),
        action(QStringLiteral("go.notifications"), "go", "Go", "Notifications", "Ctrl+4", 1, 3),
        action(QStringLiteral("go.input"), "go", "Go", "Input & macros", "Ctrl+5", 1, 4),
        action(QStringLiteral("go.vault"), "go", "Go", "Vault", "Ctrl+6", 1, 5),
        action(QStringLiteral("help.about"), "help", "Help", "About QindaLink", "F1", 2, 0),
    };
}

QString pageForActionId(const QString &actionId)
{
    return actionId.startsWith(QLatin1String("go.")) ? actionId.mid(3) : QString();
}

} // namespace QindaQt::LinkApp
