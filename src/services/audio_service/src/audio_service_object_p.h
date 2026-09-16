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
        "<method name=\"GetSnapshot\"><arg name=\"snapshot\" type=\"(uttuuss(tt)(tt)a((tt)ussdbbbbbbadasbs)a((tt)ussdbbbbbbadasbs)a((tt)uss(tt)bdbbbbbbadas)(a(suusttbdbbbdada(ubd)(ddb)s((bd)(bddddd)(bdddddd)(bddddddd)(bdd)))a(suusttbdbb(ddb)s((bddddddd)u))b))\" "
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
        "<method name=\"SetStripGain\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"gainDb\" type=\"d\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripMute\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"muted\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripSolo\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"soloed\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripMono\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"mono\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripPan\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"pan\" type=\"d\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripTrim\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"trimDb\" type=\"ad\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripSend\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"busIndex\" type=\"u\" direction=\"in\"/><arg name=\"enabled\" type=\"b\" direction=\"in\"/><arg name=\"gainDb\" type=\"d\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetBusGain\"><arg name=\"bus\" type=\"s\" direction=\"in\"/><arg name=\"gainDb\" type=\"d\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetBusMute\"><arg name=\"bus\" type=\"s\" direction=\"in\"/><arg name=\"muted\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetBusMono\"><arg name=\"bus\" type=\"s\" direction=\"in\"/><arg name=\"mono\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetBusTarget\"><arg name=\"bus\" type=\"s\" direction=\"in\"/><arg name=\"device\" type=\"(tt)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripSource\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"device\" type=\"(tt)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetBusProcessing\"><arg name=\"bus\" type=\"s\" direction=\"in\"/><arg name=\"processing\" type=\"((bddddddd)u)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<method name=\"SetStripProcessing\"><arg name=\"strip\" type=\"s\" direction=\"in\"/><arg name=\"processing\" type=\"((bd)(bddddd)(bdddddd)(bddddddd)(bdd))\" direction=\"in\"/><arg name=\"result\" type=\"(uuttttss)\" direction=\"out\"/></method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg "
        "name=\"revision\" type=\"t\"/></signal>"
        "<signal name=\"Levels\"><arg name=\"levels\" type=\"a(s(ddb))\"/>"
        "</signal></interface>")

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

    // Console operations (ADR-0173). Each addresses a strip or bus by its
    // stable console id, so a call stays meaningful across the device behind
    // it disappearing and coming back.
    Q_SCRIPTABLE void SetStripGain(const QString &strip, double gainDb);
    Q_SCRIPTABLE void SetStripMute(const QString &strip, bool muted);
    Q_SCRIPTABLE void SetStripSolo(const QString &strip, bool soloed);
    Q_SCRIPTABLE void SetStripMono(const QString &strip, bool mono);
    Q_SCRIPTABLE void SetStripPan(const QString &strip, double pan);
    Q_SCRIPTABLE void SetStripTrim(const QString &strip, const QVector<double> &trimDb);
    Q_SCRIPTABLE void SetStripSend(const QString &strip, quint32 busIndex,
                                   bool enabled, double gainDb);
    Q_SCRIPTABLE void SetBusGain(const QString &bus, double gainDb);
    Q_SCRIPTABLE void SetBusMute(const QString &bus, bool muted);
    Q_SCRIPTABLE void SetBusMono(const QString &bus, bool mono);
    Q_SCRIPTABLE void SetBusTarget(const QString &bus,
                                   const QindaQt::Audio::Handle &device);
    // Pins a strip to a capture device (ADR-0178); an invalid handle returns
    // the strip to automatic binding.
    Q_SCRIPTABLE void SetStripSource(const QString &strip,
                                     const QindaQt::Audio::Handle &device);
    // Replaces the strip's whole processing rack (ADR-0179).
    Q_SCRIPTABLE void SetStripProcessing(const QString &strip,
                                         const QindaQt::Audio::StripProcessing &processing);
    // Replaces the bus's rack (ADR-0180).
    Q_SCRIPTABLE void SetBusProcessing(const QString &bus,
                                       const QindaQt::Audio::BusProcessing &processing);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint64 revision);
    // Meter readings at meter rate. Deliberately NOT accompanied by a lineage
    // change: a client applies these onto the snapshot it already holds, and a
    // missed batch is simply a skipped frame of animation.
    Q_SCRIPTABLE void Levels(const QList<QindaQt::Audio::LevelReading> &levels);

private:
    void beginOperation(const OperationRequest &request);
    void finishOperation(quint64 operationId, const OperationResult &result);

    AudioOperationCoordinator *m_coordinator = nullptr;
    QDBusConnection m_connection;
    QHash<quint64, QDBusMessage> m_pendingReplies;
};

} // namespace QindaQt::Audio
