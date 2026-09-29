// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositorconfigimport.h"

#include "qindaqt/compositor_names/compositor_names.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace QindaQt::Session {

QList<CompositorConfigImport::Pair> CompositorConfigImport::pairs()
{
    using namespace QindaQt::CompositorNames;
    return {
        {QStringLiteral("kwinrc"), QString(configFile), false},
        {QStringLiteral("kwinrulesrc"), QString(rulesFile), false},
        {QStringLiteral("kwinoutputconfig.json"), QString(outputConfigFile), false},
        {QStringLiteral("kcminputrc"), QString(inputConfigFile), false},
        {QStringLiteral("kxkbrc"), QString(keyboardConfigFile), false},
        {QStringLiteral("kwinstaterc"), QString(stateFile), true},
    };
}

bool CompositorConfigImport::run(const QString &configHome,
                                 const QString &stateHome,
                                 QStringList *imported,
                                 QString *error)
{
    for (const Pair &pair : pairs()) {
        const QString home = pair.state ? stateHome : configHome;
        if (home.trimmed().isEmpty()) {
            continue;
        }
        const QDir directory(home);
        const QString target = directory.filePath(pair.qindaqtFile);
        const QString source = directory.filePath(pair.kdeFile);
        // AGENT-GUARD: import exactly once. An existing qindaqt-kwin file is the
        // user's QindaQt setting, even when the KDE file changed since.
        if (QFileInfo::exists(target) || !QFileInfo(source).isFile()) {
            continue;
        }
        QFile input(source);
        if (!input.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = QStringLiteral("could not read '%1' to import it: %2").arg(source, input.errorString());
            }
            return false;
        }
        const QByteArray bytes = input.readAll();
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
            if (error) {
                *error = QStringLiteral("could not create '%1'").arg(QFileInfo(target).absolutePath());
            }
            return false;
        }
        QSaveFile output(target);
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
            if (error) {
                *error = QStringLiteral("could not write '%1': %2").arg(target, output.errorString());
            }
            return false;
        }
        QFile::setPermissions(target, input.permissions());
        if (imported) {
            imported->append(pair.qindaqtFile);
        }
    }
    return true;
}

} // namespace QindaQt::Session
