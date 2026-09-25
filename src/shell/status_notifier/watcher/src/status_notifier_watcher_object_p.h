// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusContext>
#include <QObject>
#include <QStringList>

namespace QindaQt::StatusNotifier
{

class StatusNotifierWatcherService;

// The exported org.kde.StatusNotifierWatcher D-Bus object. This is the only
// place that parses caller messages; all policy lives in the service. Slots
// translate a failed RegistrationAttempt into a D-Bus error reply.
//
// AGENT-NOTE: the three Q_PROPERTYs are served by QtDBus's built-in
// org.freedesktop.DBus.Properties handler (registration exports
// ExportAllProperties): Get/GetAll answer under the standard interface header
// and under an empty one, and Set is refused because every property is
// read-only. That is how Plasma, waybar and our own monitor read the watcher.
// An earlier hand-written adaptor, declared under the misspelled
// "org.freedesktop.D-Bus.Properties", advertised an invalid interface name
// and was a remote crash: a header-less Set reached its Set slot, which
// called QDBusContext::sendErrorReply, but QtDBus sets the call context only
// on the adaptor's parent, so the reply dereferenced a null context and any
// session-bus peer could segfault the shell.
//
// AGENT-GUARD: do not re-add a Properties adaptor, and never derive a
// QDBusAbstractAdaptor from QDBusContext: QtDBus never gives an adaptor a
// call context, so sendErrorReply/message()/setDelayedReply there crash.
// Pinned by the watcher row servesStandardPropertiesInterface.
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

} // namespace QindaQt::StatusNotifier
