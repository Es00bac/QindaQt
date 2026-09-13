// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_dbus.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "support/display_protocol_test_data.h"

#include <QtCore/QVariant>
#include <QtDBus/QDBusMetaType>
#include <QtTest>

#include <functional>
#include <limits>

using namespace QindaQt::Display;

namespace
{

const QString kStableId = QStringLiteral("edid:00112233445566778899aabbccddeeff");

BrightnessSnapshot brightness()
{
    return {.protocolVersion = kProtocolVersion,
            .serviceEpoch = QStringLiteral("display-epoch"),
            .topologyRevision = 7,
            .revision = 3,
            .outputs = {{.stableId = kStableId,
                         .capable = true,
                         .observed = true,
                         .value = 4'000}}};
}

BrightnessRequest request()
{
    return {.baseEpoch = QStringLiteral("display-epoch"),
            .baseRevision = 3,
            .stableId = kStableId,
            .value = 2'500};
}

template<typename T>
QDBusArgument marshalledArgument(const T &value)
{
    QDBusArgument writer;
    writer << value;
    return qvariant_cast<QDBusArgument>(QVariant::fromValue(writer));
}

} // namespace

class DisplayProtocolBrightnessTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsBoundedBrightnessValues();
    void rejectsHostileBrightnessSnapshots_data();
    void rejectsHostileBrightnessSnapshots();
    void rejectsHostileBrightnessRequests_data();
    void rejectsHostileBrightnessRequests();
    void joinsOnlyTheExactSnapshotRevision();
    void fixesDbusSignaturesAndFailsClosed();
};

void DisplayProtocolBrightnessTests::acceptsBoundedBrightnessValues()
{
    QVERIFY(validateBrightnessSnapshot(brightness()).accepted);
    QVERIFY(validateBrightnessRequest(request()).accepted);
    QVERIFY(validateBrightnessJoin(Test::snapshot(), brightness()).accepted);

    BrightnessSnapshot bounds = brightness();
    bounds.outputs[0].value = kMaxBrightness;
    QVERIFY(validateBrightnessSnapshot(bounds).accepted);
    bounds.outputs[0].value = 0;
    QVERIFY(validateBrightnessSnapshot(bounds).accepted);
    // A capability may be advertised before any value is observed; a cleared
    // row is neither capable nor observed and carries zero.
    bounds.outputs[0] = {.stableId = kStableId, .capable = true, .observed = false, .value = 0};
    QVERIFY(validateBrightnessSnapshot(bounds).accepted);
    bounds.outputs[0].capable = false;
    QVERIFY(validateBrightnessSnapshot(bounds).accepted);

    BrightnessRequest maximum = request();
    maximum.value = kMaxBrightness;
    QVERIFY(validateBrightnessRequest(maximum).accepted);
    maximum.value = 0;
    QVERIFY(validateBrightnessRequest(maximum).accepted);

    // The immediate result shares the closed OperationResult vocabulary.
    const OperationResult result{.kind = OperationKind::ImmediatePolicy,
                                 .status = OperationStatus::Succeeded,
                                 .error = ErrorCode::None,
                                 .initiatingEpoch = QStringLiteral("display-epoch"),
                                 .initiatingRevision = 3,
                                 .observedRevision = 4,
                                 .transactionId = {},
                                 .diagnostic = {}};
    QVERIFY(validateOperationResult(result).accepted);
    OperationResult regressed = result;
    regressed.observedRevision = 2;
    QCOMPARE(validateOperationResult(regressed).reasonCode,
             QStringLiteral("invalid-success-result"));
}

void DisplayProtocolBrightnessTests::rejectsHostileBrightnessSnapshots_data()
{
    using Mutation = std::function<void(BrightnessSnapshot &)>;
    QTest::addColumn<Mutation>("mutate");
    QTest::addColumn<QString>("reason");

    const auto row = [](const char *name, Mutation mutation, const char *reason) {
        QTest::newRow(name) << std::move(mutation) << QString::fromLatin1(reason);
    };
    row("wire-invalid", [](BrightnessSnapshot &v) { v.wireValid = false; }, "malformed-payload");
    row("old-version", [](BrightnessSnapshot &v) { v.protocolVersion = 0; }, "unsupported-version");
    row("new-version", [](BrightnessSnapshot &v) { v.protocolVersion = 2; }, "unsupported-version");
    row("empty-epoch", [](BrightnessSnapshot &v) { v.serviceEpoch.clear(); },
        "invalid-brightness-lineage");
    row("control-epoch", [](BrightnessSnapshot &v) { v.serviceEpoch = QString::fromUtf16(u"a\u0001b"); },
        "invalid-brightness-lineage");
    row("oversized-epoch",
        [](BrightnessSnapshot &v) { v.serviceEpoch = QString(kMaxServiceEpochUtf8Bytes + 1, u'e'); },
        "invalid-brightness-lineage");
    row("zero-topology-revision", [](BrightnessSnapshot &v) { v.topologyRevision = 0; },
        "invalid-brightness-lineage");
    row("zero-revision", [](BrightnessSnapshot &v) { v.revision = 0; }, "invalid-brightness-lineage");
    row("no-rows", [](BrightnessSnapshot &v) { v.outputs.clear(); }, "invalid-brightness-count");
    row("too-many-rows",
        [](BrightnessSnapshot &v) {
            v.outputs.clear();
            for (qsizetype index = 0; index <= kMaxOutputs; ++index) {
                v.outputs.push_back({.stableId = QStringLiteral("conn:DP-%1").arg(index)});
            }
        },
        "invalid-brightness-count");
    row("empty-stable-id", [](BrightnessSnapshot &v) { v.outputs[0].stableId.clear(); },
        "invalid-brightness-output");
    row("nul-stable-id", [](BrightnessSnapshot &v) { v.outputs[0].stableId = QString::fromUtf16(u"a\0b", 3); },
        "invalid-brightness-output");
    row("oversized-stable-id",
        [](BrightnessSnapshot &v) { v.outputs[0].stableId = QString(kMaxStableIdUtf8Bytes + 1, u's'); },
        "invalid-brightness-output");
    row("value-above-scale", [](BrightnessSnapshot &v) { v.outputs[0].value = kMaxBrightness + 1; },
        "invalid-brightness-value");
    row("value-uint-max",
        [](BrightnessSnapshot &v) { v.outputs[0].value = std::numeric_limits<quint32>::max(); },
        "invalid-brightness-value");
    row("unobserved-value", [](BrightnessSnapshot &v) { v.outputs[0].observed = false; },
        "inconsistent-brightness-output");
    row("duplicate-row", [](BrightnessSnapshot &v) { v.outputs.push_back(v.outputs.constFirst()); },
        "duplicate-brightness-output");
}

void DisplayProtocolBrightnessTests::rejectsHostileBrightnessSnapshots()
{
    using Mutation = std::function<void(BrightnessSnapshot &)>;
    QFETCH(Mutation, mutate);
    QFETCH(QString, reason);
    BrightnessSnapshot value = brightness();
    mutate(value);
    const ValidationResult validation = validateBrightnessSnapshot(value);
    QVERIFY(!validation.accepted);
    QCOMPARE(validation.reasonCode, reason);
}

void DisplayProtocolBrightnessTests::rejectsHostileBrightnessRequests_data()
{
    using Mutation = std::function<void(BrightnessRequest &)>;
    QTest::addColumn<Mutation>("mutate");
    QTest::addColumn<QString>("reason");

    const auto row = [](const char *name, Mutation mutation, const char *reason) {
        QTest::newRow(name) << std::move(mutation) << QString::fromLatin1(reason);
    };
    row("empty-epoch", [](BrightnessRequest &v) { v.baseEpoch.clear(); }, "invalid-brightness-lineage");
    row("format-epoch", [](BrightnessRequest &v) { v.baseEpoch = QString::fromUtf16(u"a\u200Eb"); },
        "invalid-brightness-lineage");
    row("zero-revision", [](BrightnessRequest &v) { v.baseRevision = 0; }, "invalid-brightness-lineage");
    row("empty-output", [](BrightnessRequest &v) { v.stableId.clear(); }, "invalid-brightness-output");
    row("oversized-output",
        [](BrightnessRequest &v) { v.stableId = QString(kMaxStableIdUtf8Bytes + 1, u's'); },
        "invalid-brightness-output");
    row("value-above-scale", [](BrightnessRequest &v) { v.value = kMaxBrightness + 1; },
        "invalid-brightness-value");
    row("value-uint-max", [](BrightnessRequest &v) { v.value = std::numeric_limits<quint32>::max(); },
        "invalid-brightness-value");
}

void DisplayProtocolBrightnessTests::rejectsHostileBrightnessRequests()
{
    using Mutation = std::function<void(BrightnessRequest &)>;
    QFETCH(Mutation, mutate);
    QFETCH(QString, reason);
    BrightnessRequest value = request();
    mutate(value);
    const ValidationResult validation = validateBrightnessRequest(value);
    QVERIFY(!validation.accepted);
    QCOMPARE(validation.reasonCode, reason);
}

void DisplayProtocolBrightnessTests::joinsOnlyTheExactSnapshotRevision()
{
    const Snapshot snapshot = Test::snapshot();

    BrightnessSnapshot otherEpoch = brightness();
    otherEpoch.serviceEpoch = QStringLiteral("replacement-epoch");
    QCOMPARE(validateBrightnessJoin(snapshot, otherEpoch).reasonCode,
             QStringLiteral("brightness-lineage-mismatch"));

    BrightnessSnapshot olderTopology = brightness();
    olderTopology.topologyRevision = snapshot.revision - 1;
    QCOMPARE(validateBrightnessJoin(snapshot, olderTopology).reasonCode,
             QStringLiteral("brightness-lineage-mismatch"));

    BrightnessSnapshot extraRow = brightness();
    extraRow.outputs.push_back({.stableId = QStringLiteral("conn:DP-9")});
    QCOMPARE(validateBrightnessJoin(snapshot, extraRow).reasonCode,
             QStringLiteral("brightness-output-mismatch"));

    Snapshot dual = snapshot;
    Output second = Test::output(QStringLiteral("conn:DP-2"), QStringLiteral("DP-2"));
    second.primary = false;
    second.priority = 2;
    dual.outputs.push_back(second);
    BrightnessSnapshot reordered = brightness();
    reordered.outputs.prepend({.stableId = QStringLiteral("conn:DP-2")});
    QCOMPARE(validateBrightnessJoin(dual, reordered).reasonCode,
             QStringLiteral("brightness-output-mismatch"));
    std::swap(reordered.outputs[0], reordered.outputs[1]);
    QVERIFY(validateBrightnessJoin(dual, reordered).accepted);

    BrightnessSnapshot invalid = brightness();
    invalid.outputs[0].value = kMaxBrightness + 1;
    QCOMPARE(validateBrightnessJoin(snapshot, invalid).reasonCode,
             QStringLiteral("invalid-brightness-value"));
}

void DisplayProtocolBrightnessTests::fixesDbusSignaturesAndFailsClosed()
{
    registerDBusTypes();
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<OutputBrightness>()), "(sbbu)");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<BrightnessSnapshot>()),
             "(ustta(sbbu))");
    QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<BrightnessRequest>()), "(stsu)");

    // Locally marshalled arguments are write-only; the positive inbound path
    // is private-bus evidence. The wrappers must reject without extraction.
    BrightnessSnapshot snapshotDestination = brightness();
    snapshotDestination.revision = 99;
    const BrightnessSnapshot snapshotPrior = snapshotDestination;
    QVERIFY(!decodeBrightnessSnapshotArgument(marshalledArgument(brightness()),
                                              snapshotDestination)
                 .accepted);
    QVERIFY(!decodeBrightnessSnapshotArgument(marshalledArgument(request()), snapshotDestination)
                 .accepted);
    QCOMPARE(snapshotDestination, snapshotPrior);

    BrightnessRequest requestDestination = request();
    requestDestination.value = 77;
    const BrightnessRequest requestPrior = requestDestination;
    QVERIFY(!decodeBrightnessRequestArgument(marshalledArgument(request()), requestDestination)
                 .accepted);
    QVERIFY(!decodeBrightnessRequestArgument(marshalledArgument(QStringLiteral("wrong")),
                                             requestDestination)
                 .accepted);
    QCOMPARE(requestDestination, requestPrior);
}

QTEST_GUILESS_MAIN(DisplayProtocolBrightnessTests)
#include "tst_display_protocol_brightness.moc"
