// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/config_change_announcement.h>

#include <QDBusMessage>
#include <QDBusMetaType>
#include <QFileInfo>
#include <QRegularExpression>

namespace QindaQt::Apps::SettingsInput {

bool announceConfigChange(const QDBusConnection &bus,
                          const QString &configFilePath,
                          const QHash<QString, QByteArrayList> &changedKeys) {
    if (!bus.isConnected() || changedKeys.isEmpty()) {
        return false;
    }
    const QString fileName = QFileInfo(configFilePath).fileName();
    // AGENT-GUARD: An object path element allows only [A-Za-z0-9_]. The
    // desktop's rc names qualify; relocated test names with other characters
    // are skipped instead of producing an invalid message, which also keeps
    // a stray test write from reaching any real desktop watcher.
    static const QRegularExpression pathElement(
        QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!pathElement.match(fileName).hasMatch()) {
        return false;
    }
    qDBusRegisterMetaType<QByteArrayList>();
    qDBusRegisterMetaType<QHash<QString, QByteArrayList>>();
    QDBusMessage message = QDBusMessage::createSignal(
        QLatin1Char('/') + fileName, QStringLiteral("org.kde.kconfig.notify"),
        QStringLiteral("ConfigChanged"));
    message.setArguments({QVariant::fromValue(changedKeys)});
    return bus.send(message);
}

} // namespace QindaQt::Apps::SettingsInput
