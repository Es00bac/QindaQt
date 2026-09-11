// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusMetaType>
#include <QtTest>

#include <limits>
#include <memory>

using namespace QindaQt::Audio;

namespace
{

Snapshot validSnapshot()
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = 19;
    snapshot.revision = 4;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capability::SetDefault | Capability::SetVolume
        | Capability::SetMute | Capability::MoveStream
        | Capability::SetChannelVolumes | Capability::ManageVirtualDevices;
    snapshot.defaultOutput = {.epoch = 19, .serial = 100};
    snapshot.defaultInput = {.epoch = 19, .serial = 200};
    snapshot.outputs = {{.handle = {.epoch = 19, .serial = 100},
                         .kind = DeviceKind::Output,
                         .name = QStringLiteral("Speakers"),
                         .description = QStringLiteral("Null sink"),
                         .volume = 0.5,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .isDefault = true,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .channelVolumes = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR"),
                                        QStringLiteral("FC"), QStringLiteral("LFE"),
                                        QStringLiteral("SL"), QStringLiteral("SR")},
                         .virtualDevice = false},
                        {.handle = {.epoch = 19, .serial = 101},
                         .kind = DeviceKind::Output,
                         .name = QStringLiteral("Virtual Bus"),
                         .description = QStringLiteral("Managed null device"),
                         .volume = 0.5,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .isDefault = false,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .channelVolumes = {0.5, 0.5},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                         .virtualDevice = true}};
    snapshot.inputs = {{.handle = {.epoch = 19, .serial = 200},
                        .kind = DeviceKind::Input,
                        .name = QStringLiteral("Microphone"),
                        .description = QStringLiteral("Null source"),
                        .volume = 0.75,
                        .volumeKnown = true,
                        .muted = true,
                        .muteKnown = true,
                        .isDefault = true,
                        .canSetVolume = true,
                        .canSetMute = true,
                        .channelVolumes = {0.75, 0.75},
                        .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                        .virtualDevice = false}};
    snapshot.streams = {{.handle = {.epoch = 19, .serial = 300},
                         .direction = StreamDirection::Playback,
                         .applicationName = QStringLiteral("Player"),
                         .mediaName = QStringLiteral("Music"),
                         .target = {.epoch = 19, .serial = 100},
                         .targetKnown = true,
                         .volume = 0.25,
                         .volumeKnown = true,
                         .muted = false,
                         .muteKnown = true,
                         .canSetVolume = true,
                         .canSetMute = true,
                         .canMove = true,
                         .channelVolumes = {0.25, 0.25},
                         .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")}}};
    return snapshot;
}
class PrivateBus final
{
public:
    bool start()
    {
        process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-audio-protocol-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

// Echoes snapshots back so the caller decodes what its own marshaller wrote,
// or replies with a preset hostile payload to exercise fail-closed decoding.
class EchoService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.AudioProtocolEcho")

public:
    explicit EchoService(const QDBusConnection &connection, QObject *parent = nullptr)
        : QObject(parent)
        , m_connection(connection)
    {
        m_connection.registerService(QStringLiteral("org.qindaqt.AudioProtocolEcho"));
        m_connection.registerObject(QStringLiteral("/org/qindaqt/AudioProtocolEcho"),
                                    this, QDBusConnection::ExportAllSlots);
    }

    void setHostileReply(Snapshot hostile)
    {
        m_hostile = std::move(hostile);
    }

public Q_SLOTS:
    Q_SCRIPTABLE QindaQt::Audio::Snapshot echo(const QindaQt::Audio::Snapshot &snapshot)
    {
        return m_hostile.has_value() ? *m_hostile : snapshot;
    }

private:
    QDBusConnection m_connection;
    std::optional<Snapshot> m_hostile;
};

std::optional<Snapshot> echoOverBus(const QDBusConnection &connection, const Snapshot &snapshot)
{
    QDBusMessage call =
        QDBusMessage::createMethodCall(QStringLiteral("org.qindaqt.AudioProtocolEcho"),
                                       QStringLiteral("/org/qindaqt/AudioProtocolEcho"),
                                       QStringLiteral("org.qindaqt.AudioProtocolEcho"),
                                       QStringLiteral("echo"));
    call.setArguments({QVariant::fromValue(snapshot)});
    QDBusPendingCallWatcher watcher(connection.asyncCall(call));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!finished.wait(5000)) {
        return std::nullopt;
    }
    const QDBusPendingReply<Snapshot> reply = watcher;
    if (reply.isError()) {
        return std::nullopt;
    }
    return reply.value();
}

} // namespace

class AudioProtocolTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fixedSignatures();
    void acceptsCanonicalSnapshot();
    void rejectsMalformedLineageAndKinds();
    void rejectsUnsortedDuplicateAndStaleHandles();
    void rejectsInvalidLevelsAndText();
    void rejectsOversizedCollections();
    void rejectsInvalidChannelTruth();
    void operationResultLineage();
    void rejectsInconsistentCapabilitiesDefaultsAndDiagnostics();
    void snapshotRoundTripsOverDBus();
    void hostileChannelArraysFailClosedOverDBus();
};

void AudioProtocolTests::fixedSignatures()
{
    registerDBusTypes();
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Handle>()), "(tt)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Device>()),
             "((tt)ussdbbbbbbadasb)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Stream>()),
             "((tt)uss(tt)bdbbbbbbadas)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Snapshot>()),
             "(uttuuss(tt)(tt)a((tt)ussdbbbbbbadasb)a((tt)ussdbbbbbbadasb)"
             "a((tt)uss(tt)bdbbbbbbadas))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<OperationResult>()),
             "(uuttttss)");
}

void AudioProtocolTests::acceptsCanonicalSnapshot()
{
    const auto validation = validateSnapshot(validSnapshot());
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));
}

void AudioProtocolTests::rejectsMalformedLineageAndKinds()
{
    Snapshot snapshot = validSnapshot();
    snapshot.schemaVersion++;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unsupported-version"));
    snapshot = validSnapshot();
    snapshot.epoch = 0;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-lineage"));
    snapshot = validSnapshot();
    snapshot.outputs[0].kind = static_cast<DeviceKind>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device-order"));
    snapshot = validSnapshot();
    snapshot.availability = static_cast<Availability>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-availability"));
}

void AudioProtocolTests::rejectsUnsortedDuplicateAndStaleHandles()
{
    Snapshot snapshot = validSnapshot();
    Device second = snapshot.outputs[0];
    second.handle.serial = 99;
    second.isDefault = false;
    snapshot.outputs.push_back(second);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.inputs[0].handle.serial = snapshot.outputs[0].handle.serial;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.streams[0].handle.epoch++;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));

    snapshot = validSnapshot();
    snapshot.streams[0].target.serial = 200;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-stream-target"));
}

void AudioProtocolTests::rejectsInvalidLevelsAndText()
{
    Snapshot snapshot = validSnapshot();
    snapshot.outputs[0].volume = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));
    snapshot = validSnapshot();
    snapshot.streams[0].volume = 1.01;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));
    snapshot = validSnapshot();
    snapshot.outputs[0].name = QString(kMaxDisplayNameUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));
    snapshot = validSnapshot();
    snapshot.reasonCode = QString(kMaxReasonCodeUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-text"));
}

void AudioProtocolTests::rejectsOversizedCollections()
{
    Snapshot snapshot = validSnapshot();
    snapshot.outputs.clear();
    snapshot.defaultOutput = {};
    for (qsizetype index = 0; index <= kMaxOutputs; ++index) {
        Device device = validSnapshot().outputs[0];
        device.handle.serial = static_cast<quint64>(index + 1);
        device.isDefault = false;
        snapshot.outputs.push_back(device);
    }
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-payload"));
    snapshot = validSnapshot();
    snapshot.wireValid = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-payload"));
}

void AudioProtocolTests::rejectsInvalidChannelTruth()
{
    Snapshot snapshot = validSnapshot();
    snapshot.outputs[0].channelVolumes[2] = 1.5;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.outputs[0].channelVolumes.push_back(0.1);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.outputs[0].channelVolumes.resize(kMaxChannelsPerDevice + 1, 0.1);
    snapshot.outputs[0].channelMap = QStringList(kMaxChannelsPerDevice + 1,
                                                 QStringLiteral("AUX"));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.outputs[0].channelMap[0] = QString(kMaxChannelNameUtf8Bytes + 1,
                                                QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.outputs[0].channelMap[0] = QString();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.streams[0].channelVolumes[1] =
        std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));

    snapshot = validSnapshot();
    snapshot.outputs[0].wireValid = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.streams[0].wireValid = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-stream"));
}

void AudioProtocolTests::operationResultLineage()
{
    OperationResult result{.kind = OperationKind::SetChannelVolumes,
                           .status = OperationStatus::Succeeded,
                           .initiatingEpoch = 3,
                           .initiatingRevision = 5,
                           .observedEpoch = 3,
                           .observedRevision = 6,
                           .reasonCode = QStringLiteral("ok"),
                           .diagnostic = {},
                           .wireValid = true};
    QVERIFY(validateOperationResult(result).accepted);
    result.kind = OperationKind::CreateVirtualDevice;
    QVERIFY(validateOperationResult(result).accepted);
    result.kind = OperationKind::RemoveVirtualDevice;
    QVERIFY(validateOperationResult(result).accepted);
    result.kind = static_cast<OperationKind>(
        static_cast<quint32>(OperationKind::RemoveVirtualDevice) + 1);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));
    result.kind = OperationKind::SetVolume;
    result.observedEpoch++;
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-success-lineage"));
    result.status = static_cast<OperationStatus>(99);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));
}

void AudioProtocolTests::rejectsInconsistentCapabilitiesDefaultsAndDiagnostics()
{
    Snapshot snapshot = validSnapshot();
    snapshot.capabilities = Capabilities::fromInt(1U << 31U);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-capabilities"));
    snapshot = validSnapshot();
    snapshot.outputs[0].isDefault = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("inconsistent-default-output"));
    snapshot = validSnapshot();
    snapshot.streams[0].targetKnown = false;
    snapshot.streams[0].target = {.epoch = 0, .serial = 100};
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("unexpected-stream-target"));
    snapshot = validSnapshot();
    snapshot.diagnostic = QString(QChar(0x0001));
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("oversized-text"));
}

void AudioProtocolTests::snapshotRoundTripsOverDBus()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    EchoService echo(bus.connection);
    const Snapshot original = validSnapshot();
    const std::optional<Snapshot> echoed = echoOverBus(bus.connection, original);
    QVERIFY(echoed.has_value());
    QCOMPARE(*echoed, original);
    QVERIFY(echoed->outputs[1].virtualDevice);
    QCOMPARE(echoed->outputs[0].channelMap,
             QStringList({QStringLiteral("FL"), QStringLiteral("FR"),
                          QStringLiteral("FC"), QStringLiteral("LFE"),
                          QStringLiteral("SL"), QStringLiteral("SR")}));
    QCOMPARE(echoed->outputs[0].channelVolumes,
             QVector<double>({0.1, 0.2, 0.3, 0.4, 0.5, 0.6}));
    QVERIFY(echoed->wireValid);
    QVERIFY(echoed->outputs[0].wireValid);
    QVERIFY(echoed->streams[0].wireValid);
}

void AudioProtocolTests::hostileChannelArraysFailClosedOverDBus()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    EchoService echo(bus.connection);

    Snapshot hostile = validSnapshot();
    hostile.outputs[0].channelVolumes =
        QVector<double>(kMaxChannelsPerDevice + 8, 0.5);
    echo.setHostileReply(hostile);
    std::optional<Snapshot> echoed = echoOverBus(bus.connection, validSnapshot());
    QVERIFY(echoed.has_value());
    QVERIFY(!echoed->wireValid);
    QVERIFY(!echoed->outputs[0].wireValid);
    QCOMPARE(echoed->outputs[0].channelVolumes.size(), kMaxChannelsPerDevice);

    hostile = validSnapshot();
    hostile.outputs[0].channelMap =
        QStringList(kMaxChannelsPerDevice + 3, QStringLiteral("FL"));
    echo.setHostileReply(hostile);
    echoed = echoOverBus(bus.connection, validSnapshot());
    QVERIFY(echoed.has_value());
    QVERIFY(!echoed->wireValid);
    QCOMPARE(echoed->outputs[0].channelMap.size(), kMaxChannelsPerDevice);

    hostile = validSnapshot();
    hostile.streams[0].channelMap[0] =
        QString(kMaxChannelNameUtf8Bytes + 10, QLatin1Char('x'));
    echo.setHostileReply(hostile);
    echoed = echoOverBus(bus.connection, validSnapshot());
    QVERIFY(echoed.has_value());
    QVERIFY(!echoed->wireValid);
    QVERIFY(echoed->streams[0].channelMap.at(0).toUtf8().size()
            <= kMaxChannelNameUtf8Bytes);
    QVERIFY(!validateSnapshot(*echoed).accepted);
}

QTEST_GUILESS_MAIN(AudioProtocolTests)
#include "tst_audio_protocol.moc"
