// SPDX-License-Identifier: GPL-3.0-or-later
#include "welcome_actions.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace QindaQt::Apps::Welcome {

WelcomeActions::WelcomeActions(QObject *parent) : QObject(parent) {}

QString WelcomeActions::resolveSibling(const QString &executable) {
    const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(executable);
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    return QStandardPaths::findExecutable(executable);
}

bool WelcomeActions::launch(const QString &action) const {
    QString executable;
    QStringList arguments;
    if (action == QLatin1String("appearance")) {
        executable = QStringLiteral("qindaqt-settings");
        arguments = {QStringLiteral("--page"), QStringLiteral("appearance")};
    } else if (action == QLatin1String("customize")) {
        executable = QStringLiteral("qindaqt-settings");
        arguments = {QStringLiteral("--page"), QStringLiteral("customize")};
    } else if (action == QLatin1String("editor")) {
        executable = QStringLiteral("qindaqt-editor");
    } else if (action == QLatin1String("files")) {
        executable = QStringLiteral("qindaqt-file-manager");
    } else {
        return false;
    }

    const QString program = resolveSibling(executable);
    return !program.isEmpty() && QProcess::startDetached(program, arguments);
}

} // namespace QindaQt::Apps::Welcome
