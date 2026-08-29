// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_service_coordinator.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

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
        "<method name=\"ReleaseProfileHold\"><arg name=\"hold\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"SetKeyboardBrightness\"><arg name=\"device\" "
        "type=\"(tt)\" direction=\"in\"/><arg name=\"value\" type=\"u\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
        "name=\"revision\" type=\"t\"/></signal></interface>")

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

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);

private:
    void beginOperation(const PowerServiceRequest &request);
    void finishOperation(quint64 operationId, const OperationResult &result);

    PowerServiceCoordinator *m_coordinator = nullptr;
    QDBusConnection m_connection;
    QHash<quint64, QDBusMessage> m_pendingReplies;
};

} // namespace QindaQt::Power
