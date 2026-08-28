// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/app_shell/application_coordinator.h"

#include <QGuiApplication>
#include <QVariantMap>

using namespace QindaQt::AppShell;

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    ApplicationCoordinator coordinator;
    coordinator.setApplicationName(QStringLiteral("Installed AppShell consumer"));

    const ActionSpec action{.id = QStringLiteral("file.quit"),
                            .menuId = QStringLiteral("file"),
                            .menuLabel = QStringLiteral("File"),
                            .label = QStringLiteral("Quit"),
                            .accessibleDescription = QStringLiteral("Close the application"),
                            .shortcut = QKeySequence(QKeySequence::Quit)};
    if (!coordinator.replaceActions({action}).ok()) {
        return 2;
    }
    const QVariantList menus = coordinator.menus();
    if (menus.size() != 1
        || menus.first().toMap().value(QStringLiteral("id")).toString()
            != QStringLiteral("file")) {
        return 3;
    }
    if (coordinator.requestOpenFile(QStringLiteral("Open document"),
                                    {QStringLiteral("text/plain")})
        == 0) {
        return 4;
    }
    return 0;
}
