// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeexposure.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridChromeExposureTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void allowsOwnersBelowOrOutsideTheAnchor();
    void blocksAnOwnerAboveTheAnchor();
    void excludesOnlyTheDraggedSource();
    void rejectsMissingOrEmptyAnchors();
};

void HybridChromeExposureTest::allowsOwnersBelowOrOutsideTheAnchor()
{
    const QVector<HybridChromeExposureEntry> stack{
        {QStringLiteral("below"), true},
        {QStringLiteral("anchor"), false},
        {QStringLiteral("above-outside"), false},
    };
    QVERIFY(sceneChromeExposed(QStringLiteral("anchor"), {}, stack));
}

void HybridChromeExposureTest::blocksAnOwnerAboveTheAnchor()
{
    const QVector<HybridChromeExposureEntry> stack{
        {QStringLiteral("anchor"), false},
        {QStringLiteral("dialog"), true},
    };
    QVERIFY(!sceneChromeExposed(QStringLiteral("anchor"), {}, stack));
}

void HybridChromeExposureTest::excludesOnlyTheDraggedSource()
{
    const QVector<HybridChromeExposureEntry> sourceOnly{
        {QStringLiteral("anchor"), false},
        {QStringLiteral("dragged"), true},
    };
    QVERIFY(sceneChromeExposed(QStringLiteral("anchor"),
                               QStringLiteral("dragged"), sourceOnly));

    const QVector<HybridChromeExposureEntry> alsoCovered{
        {QStringLiteral("anchor"), false},
        {QStringLiteral("covered"), true},
        {QStringLiteral("dragged"), true},
    };
    QVERIFY(!sceneChromeExposed(QStringLiteral("anchor"),
                                QStringLiteral("dragged"), alsoCovered));
}

void HybridChromeExposureTest::rejectsMissingOrEmptyAnchors()
{
    const QVector<HybridChromeExposureEntry> stack{
        {QStringLiteral("window"), false},
    };
    QVERIFY(!sceneChromeExposed({}, {}, stack));
    QVERIFY(!sceneChromeExposed(QStringLiteral("missing"), {}, stack));
}

QTEST_APPLESS_MAIN(HybridChromeExposureTest)

#include "tst_hybridchromeexposure.moc"
