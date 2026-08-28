// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_topology/topology.h>

#include "support/topology_test_data.h"

#include <QtTest>

using namespace QindaQt::DisplayTopology;
namespace Display = QindaQt::Display;

class TopologyGeometryTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void logicalRoundingParity_data();
    void logicalRoundingParity();
    void transformTransposition();
    void normalizationAndCoordinateBounds();
    void overlapAndGapPolicy();
};

void TopologyGeometryTests::logicalRoundingParity_data()
{
    QTest::addColumn<QSize>("pixels");
    QTest::addColumn<double>("scale");
    QTest::addColumn<QSize>("logical");
    QTest::addColumn<bool>("integral");
    QTest::newRow("1080p-125") << QSize(1920, 1080) << 1.25 << QSize(1536, 864) << true;
    QTest::newRow("1080p-150") << QSize(1920, 1080) << 1.5 << QSize(1280, 720) << true;
    QTest::newRow("wuxga-125") << QSize(1920, 1200) << 1.25 << QSize(1536, 960) << true;
    QTest::newRow("1440p-125") << QSize(2560, 1440) << 1.25 << QSize(2048, 1152) << true;
    QTest::newRow("1440p-150") << QSize(2560, 1440) << 1.5 << QSize(1707, 960) << false;
}

void TopologyGeometryTests::logicalRoundingParity()
{
    QFETCH(QSize, pixels);
    QFETCH(double, scale);
    QFETCH(QSize, logical);
    QFETCH(bool, integral);
    const Display::Mode mode{.id = QStringLiteral("mode"),
                             .pixelSize = pixels,
                             .refreshMilliHertz = 60'000,
                             .preferred = false};
    QCOMPARE(logicalSizeForMode(mode, scale, Display::Transform::Normal), logical);
    QCOMPARE(hasIntegralLogicalExtent(mode, scale, Display::Transform::Normal), integral);
}

void TopologyGeometryTests::transformTransposition()
{
    const Display::Mode mode{.id = QStringLiteral("wuxga"),
                             .pixelSize = QSize(1920, 1200),
                             .refreshMilliHertz = 60'000,
                             .preferred = false};
    QCOMPARE(logicalSizeForMode(mode, 1.0, Display::Transform::Rotate90),
             QSize(1200, 1920));
    QCOMPARE(logicalSizeForMode(mode, 1.0, Display::Transform::Rotate270),
             QSize(1200, 1920));
    QCOMPARE(logicalSizeForMode(mode, 1.0, Display::Transform::FlipX90),
             QSize(1200, 1920));
    QCOMPARE(logicalSizeForMode(mode, 1.0, Display::Transform::Rotate180),
             QSize(1920, 1200));
}

void TopologyGeometryTests::normalizationAndCoordinateBounds()
{
    Display::Snapshot snapshot = Test::dualSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    candidate.outputs[0].position = QPoint(-1920, -40);
    candidate.outputs[1].position = QPoint(0, -40);
    const ValidationResult normalized = validateAndNormalize(snapshot, candidate);
    QVERIFY2(normalized.accepted(), qPrintable(normalized.reasonCode));
    QCOMPARE(normalized.normalizedCandidate.outputs[0].position, QPoint(0, 0));
    QCOMPARE(normalized.normalizedCandidate.outputs[1].position, QPoint(1920, 0));

    candidate = Test::candidate(snapshot);
    candidate.outputs[1].position = QPoint(Display::kCoordinateBound, 0);
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::CoordinateOverflow);
    candidate.outputs[1].position = QPoint(Display::kCoordinateBound + 1, 0);
    QCOMPARE(validateAndNormalize(snapshot, candidate).error,
             TopologyError::InvalidCandidate);
}

void TopologyGeometryTests::overlapAndGapPolicy()
{
    const Display::Snapshot snapshot = Test::dualSnapshot();
    Display::Candidate candidate = Test::candidate(snapshot);
    candidate.outputs[1].position = QPoint(100, 100);
    QCOMPARE(validateAndNormalize(snapshot, candidate).error, TopologyError::Overlap);

    candidate = Test::candidate(snapshot);
    candidate.outputs[1].position = QPoint(2020, 0);
    const ValidationResult gap = validateAndNormalize(snapshot, candidate);
    QVERIFY(gap.accepted());
    QVERIFY(std::any_of(gap.warnings.cbegin(), gap.warnings.cend(),
                        [](const TopologyWarning &warning) {
                            return warning.kind == TopologyWarningKind::DisconnectedGap;
                        }));
}

QTEST_GUILESS_MAIN(TopologyGeometryTests)
#include "tst_topology_geometry.moc"
