// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/applications_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace QindaQt::Apps::FileManager::Test {

// Writes one desktop entry below an XDG data root fixture.
[[nodiscard]] inline bool writeDesktop(const QString &path, const QString &text)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(text.toUtf8()) >= 0;
}

[[nodiscard]] inline QString entryText(const QString &name, const QString &exec,
                                       const QString &categories,
                                       const QString &extra = {})
{
    return QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n"
                          "Exec=%2\nCategories=%3\n%4")
        .arg(name, exec, categories, extra);
}

// AGENT-CONTRACT: records every process-level effect an application
// activation asks for, so no row ever calls the session bus or spawns a
// process. `accept` decides the compositor's answer (ADR-0165/0172); a
// rejection carries `rejection` as its message. The recorder must outlive
// every controller built from seams().
struct RecordingLaunchSeams final
{
    bool accept = false;
    QString rejection = QStringLiteral("not a picker or docked member");
    bool spawnSucceeds = true;
    QStringList chosen;
    QList<QStringList> spawned; // program followed by its arguments

    [[nodiscard]] ApplicationLaunchSeams seams()
    {
        return {[this](const QString &entryId) {
                    chosen.append(entryId);
                    return ChooserReply{accept, accept ? QString() : rejection};
                },
                [this](const QString &program, const QStringList &arguments) {
                    spawned.append(QStringList{program} + arguments);
                    return spawnSucceeds;
                }};
    }
};

// The fixture catalog most rows share: three plain applications in three
// categories (one nested in a vendor subdirectory), one terminal and one
// D-Bus-activatable entry, and two entries the catalog must hide.
[[nodiscard]] inline bool writeSampleCatalog(const QString &root)
{
    const QString apps = root + QStringLiteral("/applications/");
    return writeDesktop(apps + QStringLiteral("editor.desktop"),
                        entryText(QStringLiteral("Editor"),
                                  QStringLiteral("/usr/bin/editor --flag \"two words\""),
                                  QStringLiteral("Development;TextEditor;"),
                                  QStringLiteral("Comment=Edits text\nIcon=accessories-text-editor\n")))
        && writeDesktop(apps + QStringLiteral("game.desktop"),
                        entryText(QStringLiteral("anagram"), QStringLiteral("/usr/bin/game"),
                                  QStringLiteral("Game;StrategyGame;")))
        && writeDesktop(apps + QStringLiteral("vendor/zeta.desktop"),
                        entryText(QStringLiteral("Zeta Viewer"), QStringLiteral("/usr/bin/zeta"),
                                  QStringLiteral("Graphics;Viewer;")))
        && writeDesktop(apps + QStringLiteral("console.desktop"),
                        entryText(QStringLiteral("Console Tool"), QStringLiteral("/usr/bin/top"),
                                  QStringLiteral("System;"), QStringLiteral("Terminal=true\n")))
        && writeDesktop(apps + QStringLiteral("bus.desktop"),
                        entryText(QStringLiteral("Bus App"), QStringLiteral("/usr/bin/bus"),
                                  QStringLiteral("Utility;"), QStringLiteral("DBusActivatable=true\n")))
        && writeDesktop(apps + QStringLiteral("handler.desktop"),
                        entryText(QStringLiteral("Hidden Handler"), QStringLiteral("/usr/bin/handler"),
                                  QStringLiteral("Utility;"), QStringLiteral("NoDisplay=true\n")))
        && writeDesktop(apps + QStringLiteral("gone.desktop"),
                        entryText(QStringLiteral("Deleted"), QStringLiteral("/usr/bin/gone"),
                                  QStringLiteral("Utility;"), QStringLiteral("Hidden=true\n")));
}

} // namespace QindaQt::Apps::FileManager::Test
