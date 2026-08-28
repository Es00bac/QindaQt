// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/display_inventory.h>

#include <qindaqt/services/display_protocol/display_validation.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_topology/topology.h>

#include "support/display_service_test_support.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

QString transformName(const Display::Transform transform)
{
    switch (transform) {
    case Display::Transform::Normal: return QStringLiteral("normal");
    case Display::Transform::Rotate90: return QStringLiteral("rotate-90");
    case Display::Transform::Rotate180: return QStringLiteral("rotate-180");
    case Display::Transform::Rotate270: return QStringLiteral("rotate-270");
    case Display::Transform::FlipX: return QStringLiteral("flip-x");
    case Display::Transform::FlipX90: return QStringLiteral("flip-x-90");
    case Display::Transform::FlipX180: return QStringLiteral("flip-x-180");
    case Display::Transform::FlipX270: return QStringLiteral("flip-x-270");
    }
    return {};
}

QJsonObject outputJson(const InventoryOutput &value)
{
    return {{QStringLiteral("name"), value.name},
            {QStringLiteral("geometry"),
             QJsonObject{{QStringLiteral("x"), value.geometry.x()},
                         {QStringLiteral("y"), value.geometry.y()},
                         {QStringLiteral("width"), value.geometry.width()},
                         {QStringLiteral("height"), value.geometry.height()}}},
            {QStringLiteral("scale"), value.scale},
            {QStringLiteral("refreshRateMilliHz"),
             static_cast<qint64>(value.refreshRateMilliHertz)},
            {QStringLiteral("transform"), transformName(value.transform)},
            {QStringLiteral("internal"), value.internal},
            {QStringLiteral("uuid"), value.runtimeCompositorUuid},
            {QStringLiteral("priority"), static_cast<qint64>(value.compositorPriority)},
            {QStringLiteral("physicalSizeMm"),
             QJsonObject{{QStringLiteral("width"),
                          value.physicalSizeMillimeters.width()},
                         {QStringLiteral("height"),
                          value.physicalSizeMillimeters.height()}}},
            {QStringLiteral("manufacturer"), value.manufacturer},
            {QStringLiteral("model"), value.model}};
}

QByteArray payload(quint64 generation, const QList<InventoryOutput> &outputs)
{
    QJsonArray array;
    for (const InventoryOutput &value : outputs) {
        array.push_back(outputJson(value));
    }
    return QJsonDocument(
               QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                           {QStringLiteral("schemaVersion"), 1},
                           {QStringLiteral("outputGeneration"),
                            QString::number(generation)},
                           {QStringLiteral("outputs"), array}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

class DisplayInventoryAdapterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void decodesAndProjectsPrivacyPreservingInventory();
    void rejectsHostileLineageAndGeometry();
    void projectsTransformAndFractionalScale();
};

void DisplayInventoryAdapterTest::decodesAndProjectsPrivacyPreservingInventory()
{
    InventoryOutput first = output();
    InventoryOutput second = output(QStringLiteral("HDMI-A-1"),
                                    QRect(1920, 0, 2560, 1440), 1.0);
    second.runtimeCompositorUuid = QStringLiteral("runtime-uuid-2");
    const InventoryDecodeResult decoded =
        decodeCompositorInventory(payload(7, {first, second}), QStringLiteral(":1.42"));
    QVERIFY(decoded.accepted());
    QCOMPARE(decoded.frame.outputGeneration, quint64(7));
    QCOMPARE(decoded.frame.outputs.size(), 2);

    const InventoryProjectionResult projected =
        projectInventory(decoded.frame, QStringLiteral("epoch-a"));
    QVERIFY2(projected.accepted(), qPrintable(projected.reasonCode));
    QVERIFY(Display::validateSnapshot(projected.snapshot).accepted);
    QCOMPARE(projected.snapshot.serviceEpoch, QStringLiteral("epoch-a"));
    QCOMPARE(projected.snapshot.revision, quint64(7));
    QCOMPARE(projected.snapshot.outputs.at(0).stableId, QStringLiteral("conn:DP-1"));
    QCOMPARE(projected.snapshot.outputs.at(1).stableId,
             QStringLiteral("conn:HDMI-A-1"));
    QVERIFY(!projected.snapshot.outputs.at(0).stableId.contains(
        QStringLiteral("runtime-uuid")));
    QCOMPARE(projected.snapshot.outputs.at(0).runtimeCompositorUuid,
             QStringLiteral("runtime-uuid"));
    QVERIFY(projected.snapshot.outputs.at(0).primary);
    QCOMPARE(projected.snapshot.outputs.at(0).priority, quint32(1));
    QCOMPARE(projected.snapshot.outputs.at(1).priority, quint32(2));
    QCOMPARE(projected.snapshot.liveFingerprint,
             DisplayTopology::canonicalFingerprint(
                 DisplayTopology::candidateFromSnapshot(projected.snapshot)));
}

void DisplayInventoryAdapterTest::rejectsHostileLineageAndGeometry()
{
    QCOMPARE(decodeCompositorInventory(payload(1, {output()}),
                                       QStringLiteral("org.qindaqt.Compositor"))
                 .error,
             InventoryError::InvalidOwner);
    QCOMPARE(decodeCompositorInventory(
                 QByteArray(kMaximumCompositorInventoryBytes + 1, 'x'),
                 QStringLiteral(":1.42"))
                 .error,
             InventoryError::PayloadTooLarge);

    QJsonObject malformed = outputJson(output());
    QJsonObject geometry = malformed.value(QStringLiteral("geometry")).toObject();
    geometry[QStringLiteral("x")] = 0.5;
    malformed[QStringLiteral("geometry")] = geometry;
    const QByteArray body = QJsonDocument(
                                QJsonObject{{QStringLiteral("status"),
                                             QStringLiteral("ok")},
                                            {QStringLiteral("schemaVersion"), 1},
                                            {QStringLiteral("outputGeneration"),
                                             QStringLiteral("1")},
                                            {QStringLiteral("outputs"),
                                             QJsonArray{malformed}}})
                                .toJson(QJsonDocument::Compact);
    QCOMPARE(decodeCompositorInventory(body, QStringLiteral(":1.42")).error,
             InventoryError::InvalidOutput);

    InventoryOutput hostileText = output();
    hostileText.name = QStringLiteral("DP-\u0001");
    QCOMPARE(decodeCompositorInventory(payload(1, {hostileText}),
                                       QStringLiteral(":1.42"))
                 .error,
             InventoryError::InvalidOutput);
    hostileText = output();
    hostileText.runtimeCompositorUuid =
        QString(Display::kMaxRuntimeUuidUtf8Bytes + 1, u'x');
    QCOMPARE(decodeCompositorInventory(payload(1, {hostileText}),
                                       QStringLiteral(":1.42"))
                 .error,
             InventoryError::InvalidOutput);

    QList<InventoryOutput> tooMany;
    for (int index = 0; index <= Display::kMaxOutputs; ++index) {
        InventoryOutput value = output(QStringLiteral("DP-%1").arg(index));
        value.runtimeCompositorUuid = QStringLiteral("uuid-%1").arg(index);
        tooMany.push_back(value);
    }
    QCOMPARE(decodeCompositorInventory(payload(1, tooMany), QStringLiteral(":1.42"))
                 .error,
             InventoryError::InvalidOutput);
}

void DisplayInventoryAdapterTest::projectsTransformAndFractionalScale()
{
    InventoryOutput value = output(QStringLiteral("DP-2"),
                                   QRect(-100, 40, 1536, 864), 1.25,
                                   Display::Transform::Rotate180);
    const InventoryProjectionResult projected =
        projectInventory(frame(3, {value}), QStringLiteral("epoch-fractional"));
    QVERIFY2(projected.accepted(), qPrintable(projected.reasonCode));
    const Display::Output &result = projected.snapshot.outputs.constFirst();
    QCOMPARE(result.position, QPoint(-100, 40));
    QCOMPARE(result.logicalSize, QSize(1536, 864));
    QCOMPARE(result.modes.constFirst().pixelSize, QSize(1920, 1080));
    QCOMPARE(result.transform, Display::Transform::Rotate180);
}

QTEST_MAIN(DisplayInventoryAdapterTest)

#include "tst_display_inventory_adapter.moc"
