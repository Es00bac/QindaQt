// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusAbstractAdaptor>
#include <QDBusContext>
#include <QDBusVariant>
#include <QMetaProperty>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::StatusNotifier
{

class StatusNotifierWatcherService;

// The exported org.kde.StatusNotifierWatcher D-Bus object. This is the only
// place that parses caller messages; all policy lives in the service. Slots
// translate a failed RegistrationAttempt into a D-Bus error reply.
class StatusNotifierWatcherObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredStatusNotifierItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isStatusNotifierHostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    explicit StatusNotifierWatcherObject(StatusNotifierWatcherService &service);

    [[nodiscard]] QStringList registeredStatusNotifierItems() const;
    [[nodiscard]] bool isStatusNotifierHostRegistered() const;
    [[nodiscard]] int protocolVersion() const;

public slots:
    void RegisterStatusNotifierItem(const QString &serviceOrPath);
    void RegisterStatusNotifierHost(const QString &service);

private:
    StatusNotifierWatcherService &m_service;
};

// AGENT-NOTE: QtDBus advertises org.freedesktop.D-Bus.Properties for objects
// registered with ExportAllProperties but never dispatches Get/GetAll/Set to
// them (Properties dispatch only works through a QDBusAbstractAdaptor, and
// only when the object is registered with ExportAdaptors). Without this
// adaptor every Properties read returns UnknownInterface, which is how real
// StatusNotifier hosts (e.g. KDE Plasma) read watcher state. Verified against
// a private-bus probe of the exact Qt 6.11.1 build we ship with.
class StatusNotifierWatcherPropertiesAdaptor final
    : public QDBusAbstractAdaptor
    , protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.D-Bus.Properties")

public:
    explicit StatusNotifierWatcherPropertiesAdaptor(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
    }

public slots:
    [[nodiscard]] QDBusVariant Get(const QString &interfaceName,
                                   const QString &propertyName)
    {
        Q_UNUSED(interfaceName);
        return QDBusVariant(parent()->property(propertyName.toUtf8().constData()));
    }

    [[nodiscard]] QVariantMap GetAll(const QString &interfaceName)
    {
        Q_UNUSED(interfaceName);
        QVariantMap values;
        const QMetaObject *meta = parent()->metaObject();
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (property.isReadable()) {
                values[QString::fromUtf8(property.name())] = property.read(parent());
            }
        }
        return values;
    }

    void Set(const QString &interfaceName,
             const QString &propertyName,
             const QDBusVariant &value)
    {
        Q_UNUSED(interfaceName);
        Q_UNUSED(propertyName);
        Q_UNUSED(value);
        // Watcher properties are read-only by protocol; refuse writes instead
        // of silently dropping them.
        sendErrorReply(QDBusError::PropertyReadOnly,
                       QStringLiteral("watcher properties are read-only"));
    }
};

} // namespace QindaQt::StatusNotifier
