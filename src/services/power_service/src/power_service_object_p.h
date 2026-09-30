// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusServiceWatcher>

#include "idle_inhibitor_registry_p.h"

namespace QindaQt::Power {

class PowerServiceObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Power1")
    // Delayed-reply slots return void at the C++ ABI, so the canonical D-Bus
    // outputs must be explicit rather than inferred from the meta-object. The
    // signatures must stay byte-identical to the fixed PB-0 marshalling in
    // power_dbus.cpp and the installed org.qindaqt.Power1.xml.
    Q_CLASSINFO(
        "D-Bus Introspection",
        "<interface name=\"org.qindaqt.Power1\">"
        "<method name=\"GetSnapshot\"><arg name=\"snapshot\" type=\"(uttuuss(bbbb"
        "bbb)(bubduubdbxbxu)a((ts)ussbbduubddbdbxbxu)(sa(ss)a((ts)sss)s)a(ssss)a("
        "(ts)sbuuub)a((ts)sbuubuuus)(bsut))\" direction=\"out\"/></method>"
        "<method name=\"SetProfile\"><arg name=\"profileId\" type=\"s\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"AcquireProfileHold\"><arg name=\"profileId\" type=\"s\" "
        "direction=\"in\"/><arg name=\"applicationName\" type=\"s\" "
        "direction=\"in\"/><arg name=\"reason\" type=\"s\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"ReleaseProfileHold\"><arg name=\"hold\" type=\"(ts)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"SetKeyboardBrightness\"><arg name=\"device\" "
        "type=\"(ts)\" direction=\"in\"/><arg name=\"value\" type=\"u\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"SetInternalBrightness\"><arg name=\"device\" "
        "type=\"(ts)\" direction=\"in\"/><arg name=\"value\" type=\"u\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"GetIdleInhibitorCapabilities\"><arg name=\"scopes\" type=\"u\" "
        "direction=\"out\"/></method>"
        "<method name=\"GetActiveIdleInhibitorScopes\"><arg name=\"scopes\" type=\"u\" "
        "direction=\"out\"/></method>"
        "<method name=\"RequestIdleInhibitorStateWithReceipt\"><arg name=\"nonce\" type=\"s\" "
        "direction=\"in\"/></method>"
        "<method name=\"AcquireIdleInhibitor\"><arg name=\"application\" type=\"s\" "
        "direction=\"in\"/><arg name=\"reason\" type=\"s\" direction=\"in\"/>"
        "<arg name=\"scopes\" type=\"u\" direction=\"in\"/><arg name=\"handle\" "
        "type=\"(ts)\" direction=\"out\"/></method>"
        "<method name=\"ReleaseIdleInhibitor\"><arg name=\"handle\" type=\"(ts)\" "
        "direction=\"in\"/><arg name=\"released\" type=\"b\" direction=\"out\"/>"
        "</method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
        "name=\"revision\" type=\"t\"/></signal>"
        "<signal name=\"IdleInhibitorsChanged\"><arg name=\"supportedScopes\" type=\"u\"/>"
        "<arg name=\"activeScopes\" type=\"u\"/></signal>"
        "<signal name=\"IdleInhibitorStateReceipt\"><arg name=\"nonce\" type=\"s\"/>"
        "<arg name=\"supportedScopes\" type=\"u\"/><arg name=\"activeScopes\" type=\"u\"/>"
        "</signal></interface>")

public:
    explicit PowerServiceObject(PowerServiceCoordinator *coordinator,
                                const QDBusConnection &connection,
                                QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE QindaQt::Power::Snapshot GetSnapshot() const;
    Q_SCRIPTABLE void SetProfile(const QString &profileId);
    Q_SCRIPTABLE void AcquireProfileHold(const QString &profileId,
                                         const QString &applicationName,
                                         const QString &reason);
    Q_SCRIPTABLE void ReleaseProfileHold(const QindaQt::Power::Handle &hold);
    Q_SCRIPTABLE void SetKeyboardBrightness(const QindaQt::Power::Handle &device,
                                            quint32 value);
    Q_SCRIPTABLE void SetInternalBrightness(const QindaQt::Power::Handle &device,
                                            quint32 value);
    Q_SCRIPTABLE quint32 GetIdleInhibitorCapabilities() const;
    Q_SCRIPTABLE quint32 GetActiveIdleInhibitorScopes() const;
    Q_SCRIPTABLE void RequestIdleInhibitorStateWithReceipt(const QString &nonce);
    Q_SCRIPTABLE QindaQt::Power::Handle AcquireIdleInhibitor(
        const QString &application, const QString &reason, quint32 scopes);
    Q_SCRIPTABLE bool ReleaseIdleInhibitor(const QindaQt::Power::Handle &handle);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);
    Q_SCRIPTABLE void IdleInhibitorsChanged(quint32 supportedScopes,
                                             quint32 activeScopes);
    Q_SCRIPTABLE void IdleInhibitorStateReceipt(const QString &nonce,
                                                 quint32 supportedScopes,
                                                 quint32 activeScopes);

private:
    void beginOperation(const PowerServiceRequest &request);
    void finishOperation(quint64 operationId, const OperationResult &result);
    [[nodiscard]] quint32 activeIdleInhibitorScopes() const;
    void synchronizeIdleInhibitorEpoch(quint64 epoch) const;

    PowerServiceCoordinator *m_coordinator = nullptr;
    QDBusConnection m_connection;
    QHash<quint64, QDBusMessage> m_pendingReplies;
    mutable IdleInhibitorRegistry m_idleInhibitors;
    QDBusServiceWatcher *m_ownerWatcher = nullptr;
    mutable QSet<QString> m_watchedOwners;
    mutable quint64 m_idleInhibitorEpoch = 0;
};

} // namespace QindaQt::Power
