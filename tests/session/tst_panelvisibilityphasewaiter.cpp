// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityphasewaiter.h"

#include <QJsonObject>
#include <QtTest>

using namespace QindaQt::Test::PanelVisibilityPhase;

namespace {

constexpr int OutputWidth = 1920;
constexpr int OutputHeight = 1200;

QJsonObject surface(int x, int y, int width, int height, int zone = 0)
{
    return {{QStringLiteral("mapped"), true},
            {QStringLiteral("committed"), true},
            {QStringLiteral("exclusiveZone"), zone},
            {QStringLiteral("geometry"),
             QJsonObject{{QStringLiteral("x"), x},
                         {QStringLiteral("y"), y},
                         {QStringLiteral("width"), width},
                         {QStringLiteral("height"), height}}}};
}

QJsonObject topPanel()
{
    return surface(0, 0, OutputWidth, 30, 30);
}

class FakeAuthority final : public SurfaceAuthority {
public:
    explicit FakeAuthority(QList<QJsonArray> snapshots)
        : m_snapshots(std::move(snapshots))
    {
    }

    QJsonArray snapshot() override
    {
        ++reads;
        if (m_snapshots.size() > 1) {
            return m_snapshots.takeFirst();
        }
        return m_snapshots.isEmpty() ? QJsonArray{} : m_snapshots.constFirst();
    }

    int reads = 0;

private:
    QList<QJsonArray> m_snapshots;
};

class FakeTimer final : public PollTimer {
public:
    explicit FakeTimer(int pollLimit) : m_pollLimit(pollLimit) {}

    bool expired() const override { return waits >= m_pollLimit; }
    void waitForNextPoll() override { ++waits; }

    int waits = 0;

private:
    int m_pollLimit;
};

PhasePredicate leftPanelHidden()
{
    return [](const QJsonArray &items) {
        const QSize output(OutputWidth, OutputHeight);
        return mappedPanel(items, QStringLiteral("top"), 30, output, 30)
            && !mappedPanel(items, QStringLiteral("left"), 40, output);
    };
}

} // namespace

class PanelVisibilityPhaseWaiterTests final : public QObject {
    Q_OBJECT

private slots:
    void zeroSizedMappedRoleWaitsForAuthoritativeAbsence();
    void zeroSizedMappedRoleThatNeverUnmapsFailsClosed();
    void escapedGeometryCannotQualifyAVisiblePhase();
};

void PanelVisibilityPhaseWaiterTests::zeroSizedMappedRoleWaitsForAuthoritativeAbsence()
{
    FakeAuthority authority({QJsonArray{topPanel(), surface(0, 30, 0, 0, 40)},
                             QJsonArray{topPanel()}});
    FakeTimer timer(3);
    QJsonArray observed;

    QVERIFY(waitForSettledPhase(authority, timer,
                                QSize(OutputWidth, OutputHeight),
                                leftPanelHidden(), &observed));
    QCOMPARE(authority.reads, 2);
    QCOMPARE(timer.waits, 1);
    QCOMPARE(observed.size(), 1);
}

void PanelVisibilityPhaseWaiterTests::zeroSizedMappedRoleThatNeverUnmapsFailsClosed()
{
    const QJsonArray transitional{topPanel(), surface(0, 30, 0, 0, 40)};
    FakeAuthority authority({transitional});
    FakeTimer timer(2);
    QJsonArray observed;

    QVERIFY(!waitForSettledPhase(authority, timer,
                                 QSize(OutputWidth, OutputHeight),
                                 leftPanelHidden(), &observed));
    QCOMPARE(authority.reads, 2);
    QCOMPARE(timer.waits, 2);
    QCOMPARE(observed, transitional);
}

void PanelVisibilityPhaseWaiterTests::escapedGeometryCannotQualifyAVisiblePhase()
{
    FakeAuthority authority({QJsonArray{topPanel(),
                                        surface(0, 30, 40, OutputHeight, 40)}});
    FakeTimer timer(1);
    QJsonArray observed;
    const auto leftVisible = [](const QJsonArray &items) {
        return mappedPanel(items, QStringLiteral("left"), 40,
                           QSize(OutputWidth, OutputHeight), 40);
    };

    QVERIFY(!waitForSettledPhase(authority, timer,
                                 QSize(OutputWidth, OutputHeight),
                                 leftVisible, &observed));
}

QTEST_GUILESS_MAIN(PanelVisibilityPhaseWaiterTests)
#include "tst_panelvisibilityphasewaiter.moc"
