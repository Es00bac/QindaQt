// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArrayList>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>

namespace QindaQt::Tests
{

// Records org.kde.kconfig.notify ConfigChanged signals for one config name on
// a private bus, decoded the way KConfigWatcher decodes them. Listen on a
// second connection so the port's own connection is never the receiver.
class ConfigChangeListener final : public QObject
{
    Q_OBJECT

public:
    bool listen(QDBusConnection connection, const QString &configName)
    {
        qDBusRegisterMetaType<QByteArrayList>();
        qDBusRegisterMetaType<QHash<QString, QByteArrayList>>();
        m_connection = connection;
        return m_connection.connect(QString(), QLatin1Char('/') + configName,
                                    QStringLiteral("org.kde.kconfig.notify"),
                                    QStringLiteral("ConfigChanged"), this,
                                    SLOT(record(QDBusMessage)));
    }

    QList<QHash<QString, QByteArrayList>> changes;

private Q_SLOTS:
    void record(const QDBusMessage &message)
    {
        if (message.arguments().size() != 1) {
            return;
        }
        changes.append(qdbus_cast<QHash<QString, QByteArrayList>>(
            message.arguments().at(0)));
    }

private:
    QDBusConnection m_connection{QStringLiteral("none")};
};

} // namespace QindaQt::Tests
