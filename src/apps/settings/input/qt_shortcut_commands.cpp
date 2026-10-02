// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/services/shortcuts_client/transport.h>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
namespace QindaQt::Apps::SettingsInput {
namespace {
Q_LOGGING_CATEGORY(lcShortcutPort, "qindaqt.settings.input.command_shortcuts")
constexpr int MaximumNameLength = 128, MaximumCommandLength = 512;
constexpr auto CommandDirectory = "kglobalaccel";
bool validIdentity(const QString &id) { return !id.isEmpty() && id.size() <= 512 && !id.contains('/') && !id.contains(QChar(0x1f)); }
}
bool QtShortcutPort::addCommandShortcut(const QString &name,
                                        const QString &command,
                                        const QList<QKeySequence> &keys,
                                        QString *componentUnique,
                                        QString *error) const {
    const QString trimmedName = name.trimmed();
    const QString trimmedCommand = command.trimmed();
    if (trimmedName.isEmpty() || trimmedName.size() > MaximumNameLength ||
        trimmedName.contains(QLatin1Char('\n')) || trimmedCommand.isEmpty() ||
        trimmedCommand.size() > MaximumCommandLength ||
        trimmedCommand.contains(QLatin1Char('\n'))) {
        if (error != nullptr) {
            *error = QStringLiteral("The command name and command line must "
                                    "be short, single-line, and non-empty");
        }
        return false;
    }
    QString storage;
    for (const QChar c : trimmedName) {
        if (c.isLetterOrNumber()) {
            storage.append(c.toLower());
        } else if (c == QLatin1Char(' ') || c == QLatin1Char('-') ||
                   c == QLatin1Char('_')) {
            storage.append(QLatin1Char('-'));
        }
    }
    while (storage.endsWith(QLatin1Char('-'))) {
        storage.chop(1);
    }
    if (storage.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The command name needs at least one "
                                    "letter or digit");
        }
        return false;
    }
    if (!QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).available()) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.qindaqt.Shortcuts1 is not reachable");
        }
        return false;
    }
    const QString id = QLatin1String(CommandComponentPrefix) + storage +
                       QStringLiteral(".desktop");
    QDir directory(
        QDir(m_dataHome).absoluteFilePath(QLatin1String(CommandDirectory)));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create %1")
                         .arg(directory.absolutePath());
        }
        return false;
    }
    const QString filePath = directory.absoluteFilePath(id);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error != nullptr) {
            *error = QFile::exists(filePath)
                         ? QStringLiteral("A command shortcut named %1 "
                                          "already exists")
                               .arg(trimmedName)
                         : QStringLiteral("Could not write %1").arg(filePath);
        }
        return false;
    }
    // AGENT-CONTRACT: the native authority retains the legacy kglobalaccel
    // desktop-file directory and command marker, then invokes `_launch` through
    // the adapted KService/KIO boundary. Keep Exec interpretation in that
    // authority; Settings only creates the descriptor (ADR-0334).
    const QByteArray contents =
        QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\n"
                       "NoDisplay=true\nX-KDE-GlobalAccel-CommandShortcut=true\n")
            .arg(trimmedName, trimmedCommand)
            .toUtf8();
    const bool wrote = file.write(contents) == contents.size();
    file.close();
    bool ok = wrote;
    if (ok) {
        QindaQt::Services::Shortcuts::Binding binding;
        binding.component = id; binding.action = QLatin1String(LaunchActionName); binding.description = trimmedName;
        binding.keys = keys;
        ok = QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).registerBinding(binding, false, error);
    } else if (error != nullptr) {
        *error = QStringLiteral("Could not write %1").arg(filePath);
    }
    if (ok && !keys.isEmpty()) {
        ok = setShortcuts(id, QLatin1String(LaunchActionName), keys, error);
    }
    if (!ok) {
        // AGENT-GUARD: No registered-but-unassignable component and no orphan
        // file survive a failed add; listing and authority stay consistent.
        QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).unregisterBinding(id, QLatin1String(LaunchActionName));
        QFile::remove(filePath);
        qCInfo(lcShortcutPort, "removed command component %s after a failed add",
               qPrintable(id));
        return false;
    }
    if (componentUnique != nullptr) {
        *componentUnique = id;
    }
    return true;
}

bool QtShortcutPort::removeCommandShortcut(const QString &componentUnique,
                                           QString *error) const {
    ShortcutAction candidate;
    candidate.componentUnique = componentUnique;
    if (!validIdentity(componentUnique) || !candidate.isCommandComponent()) {
        if (error != nullptr) {
            *error = QStringLiteral("Only command shortcuts can be removed");
        }
        return false;
    }
    const QString filePath = QDir(m_dataHome).absoluteFilePath(
        QStringLiteral("%1/%2").arg(QLatin1String(CommandDirectory),
                                    componentUnique));
    if (!QFile::exists(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Command component %1 does not exist")
                         .arg(componentUnique);
        }
        return false;
    }
    if (!QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).available()) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Shortcut authority org.qindaqt.Shortcuts1 is not reachable");
        }
        return false;
    }
    if (!QindaQt::Services::Shortcuts::QtShortcutTransport(m_bus).unregisterBinding(componentUnique, QLatin1String(LaunchActionName), error)) return false;
    if (!QFile::remove(filePath)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not remove %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Apps::SettingsInput
