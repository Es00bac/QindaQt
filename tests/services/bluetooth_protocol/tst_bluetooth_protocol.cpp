// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QtTest>

using namespace QindaQt::Bluetooth;

namespace
{

// Locally marshalled arguments are write-only, so they cannot be demarshalled
// here; positive decode is proven by the private-bus rows against the real
// service. What this seam does prove is that the registered D-Bus writers
// execute and emit exactly the canonical ABI signature, which is the drift
// class that breaks introspection and the shipped XML.
template<typename T>
QDBusArgument marshalledArgument(const T &value)
{
    QDBusArgument writer;
    writer << value;
    return qvariant_cast<QDBusArgument>(QVariant::fromValue(writer));
}

Snapshot validSnapshot()
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = 31;
    snapshot.revision = 6;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capability::SetAdapterPower | Capability::DiscoveryLease
        | Capability::ConnectPaired | Capability::DisconnectPaired;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.adapters = {{.handle = {.epoch = 31, .serial = 400},
                         .address = QStringLiteral("AA:BB:CC:00:11:22"),
                         .name = QStringLiteral("Internal adapter"),
                         .powered = true,
                         .discovering = false}};
    snapshot.devices = {{.handle = {.epoch = 31, .serial = 700},
                         .adapterHandle = {.epoch = 31, .serial = 400},
                         .address = QStringLiteral("AA:BB:CC:33:44:55"),
                         .name = QStringLiteral("Keyboard"),
                         .deviceClass = DeviceClass::Keyboard,
                         .role = DeviceRole::Peripheral,
                         .paired = true,
                         .connected = false,
                         .rssiKnown = true,
                         .rssi = -52,
                         .batteryKnown = true,
                         .batteryPercent = 87}};
    return snapshot;
}

OperationResult validResult()
{
    return {.kind = OperationKind::Connect,
            .status = OperationStatus::Succeeded,
            .initiatingEpoch = 31,
            .initiatingRevision = 6,
            .observedEpoch = 31,
            .observedRevision = 7,
            .reasonCode = QStringLiteral("connected"),
            .diagnostic = {},
            .wireValid = true};
}

} // namespace

class BluetoothProtocolTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fixedSignatures();
    void realWireMarshallingMatchesCanonicalSignatures();
    void metaTypeRoundTripPreservesValues();
    void acceptsCanonicalSnapshot();
    void rejectsMalformedLineageAndKinds();
    void rejectsUnsortedDuplicateAndStaleHandles();
    void rejectsNonCanonicalAddresses();
    void rejectsInvalidRssiBatteryRoleAndText();
    void rejectsInconsistentDeviceAndAdapterState();
    void rejectsUnknownCapabilityBits();
    void rejectsOversizedCollections();
    void rejectsUnstructuredReasonCodes();
    void operationResultLineage();
    void rejectsMalformedOperationRequests();
};

void BluetoothProtocolTests::fixedSignatures()
{
    registerDBusTypes();
    // AGENT-GUARD: These exact signatures are the Bluetooth1 v1 ABI derived
    // from the registered codecs. They must stay identical to the adaptor
    // Q_CLASSINFO introspection and data/org.qindaqt.Bluetooth1.xml.
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Handle>()), "(tt)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Adapter>()), "((tt)ssbb)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Device>()),
             "((tt)(tt)ssuubbbnby)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<Snapshot>()),
             "(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnby))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<OperationResult>()),
             "(uuttttss)");
}

void BluetoothProtocolTests::realWireMarshallingMatchesCanonicalSignatures()
{
    registerDBusTypes();
    // Exercise the actual registered D-Bus writers and compare the produced
    // wire signature, not just the meta-type registration.
    QCOMPARE(marshalledArgument(Handle{.epoch = 1, .serial = 2}).currentSignature(),
             "(tt)");
    QCOMPARE(marshalledArgument(validSnapshot().adapters.constFirst()).currentSignature(),
             "((tt)ssbb)");
    QCOMPARE(marshalledArgument(validSnapshot().devices.constFirst()).currentSignature(),
             "((tt)(tt)ssuubbbnby)");
    QCOMPARE(marshalledArgument(validSnapshot()).currentSignature(),
             "(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnby))");
    QCOMPARE(marshalledArgument(validResult()).currentSignature(), "(uuttttss)");
}

void BluetoothProtocolTests::metaTypeRoundTripPreservesValues()
{
    registerDBusTypes();
    // Registration-level round trip through Qt's meta-type system (the same
    // machinery QtDBus uses when moving values across a connection).
    const Snapshot original = validSnapshot();
    const Snapshot decoded = QVariant::fromValue(original).value<Snapshot>();
    QCOMPARE(decoded, original);

    const OperationResult originalResult = validResult();
    const OperationResult decodedResult =
        QVariant::fromValue(originalResult).value<OperationResult>();
    QCOMPARE(decodedResult, originalResult);
}

void BluetoothProtocolTests::acceptsCanonicalSnapshot()
{
    const auto validation = validateSnapshot(validSnapshot());
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));
}

void BluetoothProtocolTests::rejectsMalformedLineageAndKinds()
{
    Snapshot snapshot = validSnapshot();
    snapshot.schemaVersion++;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unsupported-version"));

    snapshot = validSnapshot();
    snapshot.epoch = 0;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-lineage"));

    snapshot = validSnapshot();
    snapshot.devices[0].deviceClass = static_cast<DeviceClass>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.availability = static_cast<Availability>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-availability"));
}

void BluetoothProtocolTests::rejectsUnsortedDuplicateAndStaleHandles()
{
    Snapshot snapshot = validSnapshot();
    snapshot.adapters[0].handle.serial = 900;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    Device duplicate = snapshot.devices[0];
    duplicate.handle.serial = snapshot.adapters[0].handle.serial;
    duplicate.address = QStringLiteral("AA:BB:CC:33:44:66");
    snapshot.devices.push_back(duplicate);
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.devices[0].handle.epoch = snapshot.epoch + 1;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));

    snapshot = validSnapshot();
    snapshot.devices[0].adapterHandle.serial = 999;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-device-order"));
}

void BluetoothProtocolTests::rejectsNonCanonicalAddresses()
{
    const QStringList hostile = {
        QString{},
        QStringLiteral("aa:bb:cc:00:11:22"),
        QStringLiteral("AA:BB:CC:00:11:2"),
        QStringLiteral("AA:BB:CC-00-11-22"),
        QStringLiteral("AA:BB:CC:00:11:2G"),
        QStringLiteral(" AA:BB:CC:00:11:22"),
        QStringLiteral("AA:BB:CC:00:11:22\n"),
        QStringLiteral("0A:1B:2C:3D:4E:5F:6A"),
    };
    for (const QString &address : hostile) {
        QVERIFY2(!isCanonicalAddress(address), qPrintable(address));
    }
    QVERIFY(isCanonicalAddress(QStringLiteral("0A:1B:2C:3D:4E:5F")));

    Snapshot snapshot = validSnapshot();
    snapshot.adapters[0].address = QStringLiteral("aa:bb:cc:00:11:22");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-adapter"));

    snapshot = validSnapshot();
    snapshot.devices[0].address = QStringLiteral("AA.BB.CC.33.44.55");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));
}

void BluetoothProtocolTests::rejectsInvalidRssiBatteryRoleAndText()
{
    Snapshot snapshot = validSnapshot();
    snapshot.devices[0].rssiKnown = false;
    snapshot.devices[0].rssi = -40;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.devices[0].rssi = 20;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.devices[0].rssi = -200;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    // Battery is optional but never fabricatable: unknown must be zero and a
    // reported percentage must stay within BlueZ's [0, 100] range.
    snapshot = validSnapshot();
    snapshot.devices[0].batteryKnown = false;
    snapshot.devices[0].batteryPercent = 40;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.devices[0].batteryPercent = 101;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.devices[0].role = static_cast<DeviceRole>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.adapters[0].name = QString(kMaxAdapterNameUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-adapter"));

    snapshot = validSnapshot();
    snapshot.devices[0].name = QString(kMaxDeviceNameUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device"));

    snapshot = validSnapshot();
    snapshot.diagnostic = QStringLiteral("text\u0007control");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-text"));
}

void BluetoothProtocolTests::rejectsInconsistentDeviceAndAdapterState()
{
    Snapshot snapshot = validSnapshot();
    snapshot.devices[0].connected = true;
    snapshot.devices[0].paired = false;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device-state"));

    snapshot = validSnapshot();
    snapshot.adapters[0].powered = false;
    snapshot.devices[0].connected = true;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-device-state"));

    // A known-but-disconnected device may remain listed on an unpowered
    // adapter; BlueZ keeps known devices while powered off.
    snapshot = validSnapshot();
    snapshot.adapters[0].powered = false;
    QVERIFY(validateSnapshot(snapshot).accepted);

    snapshot = validSnapshot();
    snapshot.adapters[0].powered = false;
    snapshot.adapters[0].discovering = true;
    snapshot.devices.clear();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-adapter-state"));
}

void BluetoothProtocolTests::rejectsUnknownCapabilityBits()
{
    Snapshot snapshot = validSnapshot();
    snapshot.capabilities |= Capabilities::fromInt(1U << 20U);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-capabilities"));

    snapshot = validSnapshot();
    snapshot.availability = Availability::Unavailable;
    snapshot.capabilities = Capability::SetAdapterPower;
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("inconsistent-capabilities"));

    snapshot = validSnapshot();
    snapshot.availability = Availability::Unavailable;
    snapshot.adapters.clear();
    snapshot.devices.clear();
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("inconsistent-capabilities"));
}

void BluetoothProtocolTests::rejectsOversizedCollections()
{
    Snapshot snapshot = validSnapshot();
    snapshot.devices[0].handle.serial = 701;
    snapshot.devices.push_back(snapshot.devices[0]);
    snapshot.devices[1].address = QStringLiteral("AA:BB:CC:33:44:66");
    snapshot.devices[1].handle.serial = 702;
    QVERIFY(validateSnapshot(snapshot).accepted);

    Snapshot oversized = validSnapshot();
    Device device = oversized.devices[0];
    oversized.devices.clear();
    for (int index = 0; index <= kMaxDevices; ++index) {
        device.handle.serial = 1000 + static_cast<quint64>(index);
        device.address = QStringLiteral("AA:BB:CC:33:%1:%2")
                             .arg((index / 256) & 0xFF, 2, 16, QLatin1Char('0'))
                             .arg(index % 256, 2, 16, QLatin1Char('0'))
                             .toUpper();
        oversized.devices.push_back(device);
    }
    oversized.wireValid = false;
    QCOMPARE(validateSnapshot(oversized).reasonCode, QStringLiteral("oversized-payload"));
    oversized.wireValid = true;
    QCOMPARE(validateSnapshot(oversized).reasonCode, QStringLiteral("oversized-payload"));
}

void BluetoothProtocolTests::rejectsUnstructuredReasonCodes()
{
    const QStringList hostile = {
        QString{},
        QStringLiteral("Not A Token"),
        QStringLiteral("reason_code"),
        QStringLiteral("reason.code"),
        QStringLiteral("reason\u0007code"),
        QString(kMaxReasonCodeUtf8Bytes + 1, QLatin1Char('a')),
    };
    for (const QString &reason : hostile) {
        QVERIFY2(!isStructuredReasonCode(reason), qPrintable(reason));
    }
    QVERIFY(isStructuredReasonCode(QStringLiteral("adapter-off")));

    Snapshot snapshot = validSnapshot();
    snapshot.reasonCode = QStringLiteral("Not A Token");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("oversized-text"));

    OperationResult result = validResult();
    result.reasonCode = QStringLiteral("reason_code");
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));
}

void BluetoothProtocolTests::operationResultLineage()
{
    const auto validation = validateOperationResult(validResult());
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));

    OperationResult result = validResult();
    result.status = OperationStatus::Succeeded;
    result.observedRevision = result.initiatingRevision - 1;
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-success-lineage"));

    result = validResult();
    result.initiatingEpoch = 0;
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));

    result = validResult();
    result.status = static_cast<OperationStatus>(99);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));

    result = validResult();
    result.wireValid = false;
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("malformed-result"));
}

void BluetoothProtocolTests::rejectsMalformedOperationRequests()
{
    OperationRequest request{.kind = OperationKind::Connect,
                             .target = {.epoch = 31, .serial = 700},
                             .powered = false};
    QVERIFY(validateOperationRequest(request).accepted);

    request.kind = static_cast<OperationKind>(99);
    QCOMPARE(validateOperationRequest(request).reasonCode,
             QStringLiteral("malformed-request"));

    request.kind = OperationKind::Connect;
    request.target = {};
    QCOMPARE(validateOperationRequest(request).reasonCode,
             QStringLiteral("stale-handle"));
}

QTEST_GUILESS_MAIN(BluetoothProtocolTests)
#include "tst_bluetooth_protocol.moc"
