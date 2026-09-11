// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Audio
{

class AudioServiceObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Audio1")
    // Delayed-reply slots return void at the C++ ABI, so the canonical D-Bus
    // outputs must be explicit rather than inferred from the meta-object.
    Q_CLASSINFO(
        "D-Bus Introspection",
        "<interface name=\"org.qindaqt.Audio1\">"
        "<method name=\"GetSnapshot\"><arg name=\"snapshot\" type=\"(uttuuss(tt)(tt)"
        "a((tt)ussdbbbbbbadasb)a((tt)ussdbbbbbbadasb)a((tt)uss(tt)bdbbbbbbadas))\" "
        "direction=\"out\"/></method>"
        "<method name=\"SetDefault\"><arg name=\"device\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<method name=\"SetVolume\"><arg name=\"target\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"volume\" type=\"d\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetMute\"><arg name=\"target\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"muted\" type=\"b\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"MoveStream\"><arg name=\"stream\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"device\" type=\"(tt)\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetChannelVolumes\"><arg name=\"target\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"volumes\" type=\"ad\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"CreateVirtualDevice\"><arg name=\"kind\" type=\"u\" "
        "direction=\"in\"/><arg name=\"displayName\" type=\"s\" direction=\"in\"/>"
        "<arg name=\"channels\" type=\"u\" direction=\"in\"/>"
        "<arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"RemoveVirtualDevice\"><arg name=\"device\" type=\"(tt)\" "
        "direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" "
        "direction=\"out\"/></method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
        "name=\"revision\" type=\"t\"/></signal></interface>")

public:
    explicit AudioServiceObject(AudioOperationCoordinator *coordinator,
                                const QDBusConnection &connection,
                                QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE QindaQt::Audio::Snapshot GetSnapshot() const;
    Q_SCRIPTABLE void SetDefault(const QindaQt::Audio::Handle &device);
    Q_SCRIPTABLE void SetVolume(const QindaQt::Audio::Handle &target, double volume);
    Q_SCRIPTABLE void SetMute(const QindaQt::Audio::Handle &target, bool muted);
    Q_SCRIPTABLE void MoveStream(const QindaQt::Audio::Handle &stream,
                                const QindaQt::Audio::Handle &device);
    Q_SCRIPTABLE void SetChannelVolumes(const QindaQt::Audio::Handle &target,
                                        const QVector<double> &volumes);
    Q_SCRIPTABLE void CreateVirtualDevice(quint32 kind, const QString &displayName,
                                          quint32 channels);
    Q_SCRIPTABLE void RemoveVirtualDevice(const QindaQt::Audio::Handle &device);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);

private:
    void beginOperation(const OperationRequest &request);
    void finishOperation(quint64 operationId, const OperationResult &result);

    AudioOperationCoordinator *m_coordinator = nullptr;
    QDBusConnection m_connection;
    QHash<quint64, QDBusMessage> m_pendingReplies;
};

} // namespace QindaQt::Audio
