// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinoutputinventory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>
#include <utility>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

OutputInventoryEntry output(QString name = QStringLiteral("DP-1"))
{
    return {.name = std::move(name),
            .geometry = QRectF(-1920, 0, 1920, 1080),
            .visibilityGeometry = QRect(-1920, 0, 1920, 1080),
            .scale = 1.25,
            .refreshRateMilliHz = 60'000,
            .transform = QStringLiteral("normal"),
            .internal = false,
            .uuid = QStringLiteral("uuid-1"),
            .priority = std::numeric_limits<quint32>::max(),
            .physicalSizeMillimeters = QSize(600, 340),
            .manufacturer = QStringLiteral("Example"),
            .model = QStringLiteral("Panel")};
}

QJsonObject json(const QByteArray &payload)
{
    return QJsonDocument::fromJson(payload).object();
}

} // namespace

class KWinOutputInventoryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesStableCompleteProjection();
    void rejectsAmbiguityAndPreservesPriorGeneration();
    void acceptsWireBoundsAndUnconstrainedPriorities();
    void rejectsOverflowWithoutPublishing();
};

void KWinOutputInventoryTest::publishesStableCompleteProjection()
{
    OutputInventoryStore store;
    QString error;
    const QVector candidate{output()};
    QCOMPARE(store.publish(candidate, &error), OutputInventoryPublishResult::Published);
    QCOMPARE(store.generation(), quint64(1));
    const auto first = store.responseJson();
    const auto root = json(first);
    QCOMPARE(root.value(QStringLiteral("outputGeneration")).toString(), QStringLiteral("1"));
    const auto item = root.value(QStringLiteral("outputs")).toArray().at(0).toObject();
    QCOMPARE(item.value(QStringLiteral("uuid")).toString(), QStringLiteral("uuid-1"));
    QCOMPARE(item.value(QStringLiteral("priority")).toInteger(),
             qint64(std::numeric_limits<quint32>::max()));
    QCOMPARE(item.value(QStringLiteral("manufacturer")).toString(),
             QStringLiteral("Example"));
    QCOMPARE(store.publish(candidate, &error), OutputInventoryPublishResult::Unchanged);
    QCOMPARE(store.generation(), quint64(1));
    QCOMPARE(store.responseJson(), first);

    auto changed = candidate;
    changed[0].model = QStringLiteral("Panel v2");
    QCOMPARE(store.publish(changed, &error), OutputInventoryPublishResult::Published);
    QCOMPARE(store.generation(), quint64(2));

    changed[0].visibilityGeometry.translate(1, 0);
    QCOMPARE(store.publish(changed, &error), OutputInventoryPublishResult::Published);
    QCOMPARE(store.generation(), quint64(3));
}

void KWinOutputInventoryTest::rejectsAmbiguityAndPreservesPriorGeneration()
{
    OutputInventoryStore store;
    QString error;
    QVERIFY(store.publish({output()}, &error) == OutputInventoryPublishResult::Published);
    const auto prior = store.responseJson();
    auto duplicate = output(QStringLiteral("DP-2"));
    duplicate.priority = 0;
    QCOMPARE(store.publish({output(), duplicate}, &error),
             OutputInventoryPublishResult::Rejected);
    QCOMPARE(store.generation(), quint64(1));
    QCOMPARE(store.responseJson(), prior);

    duplicate.uuid = QStringLiteral("uuid-2");
    duplicate.name = QStringLiteral("DP-1");
    QCOMPARE(store.publish({output(), duplicate}, &error),
             OutputInventoryPublishResult::Rejected);
    QCOMPARE(store.responseJson(), prior);
}

void KWinOutputInventoryTest::acceptsWireBoundsAndUnconstrainedPriorities()
{
    QVector<OutputInventoryEntry> outputs;
    for (qsizetype index = 0; index < OutputInventoryStore::MaxOutputs; ++index) {
        auto entry = output(QStringLiteral("OUT-%1").arg(index));
        entry.uuid = QStringLiteral("uuid-%1").arg(index);
        entry.scale = index == 0 ? OutputInventoryStore::MaximumScale : 1.0;
        entry.priority = index < 2 ? std::numeric_limits<quint32>::max()
                                   : static_cast<quint32>(index);
        outputs.append(entry);
    }
    outputs[0].name = QString(OutputInventoryStore::MaxNameCharacters,
                              QLatin1Char('x'));
    OutputInventoryStore store;
    QCOMPARE(store.publish(outputs), OutputInventoryPublishResult::Published);
    outputs.append(output(QStringLiteral("too-many")));
    QCOMPARE(store.publish(outputs), OutputInventoryPublishResult::Rejected);
}

void KWinOutputInventoryTest::rejectsOverflowWithoutPublishing()
{
    OutputInventoryStore store({std::numeric_limits<quint64>::max()});
    QCOMPARE(store.publish({output()}),
             OutputInventoryPublishResult::GenerationExhausted);
    QVERIFY(!store.available());
}

QTEST_GUILESS_MAIN(KWinOutputInventoryTest)
#include "tst_kwinoutputinventory.moc"
