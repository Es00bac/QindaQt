// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridiconifycontroller.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

class FakeIconifyPlatform final : public HybridIconifyPlatform
{
public:
    QStringList hidden;
    QStringList failingHide;
    QStringList failingShow;
    QStringList calls;

    bool hideWindow(const QString &windowId, QString *error) override
    {
        calls.append(QStringLiteral("hide:%1").arg(windowId));
        if (failingHide.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel hide failure for %1").arg(windowId);
            }
            return false;
        }
        if (!hidden.contains(windowId)) {
            hidden.append(windowId);
        }
        return true;
    }

    bool showWindow(const QString &windowId, QString *error) override
    {
        calls.append(QStringLiteral("show:%1").arg(windowId));
        if (failingShow.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel show failure for %1").arg(windowId);
            }
            return false;
        }
        hidden.removeAll(windowId);
        return true;
    }
};

const QRectF WindowFrame(100.0, 120.0, 640.0, 480.0);
const QRectF ChipFrame(100.0, 120.0, 48.0, 48.0);
const QRectF Output(0.0, 0.0, 1920.0, 1080.0);

} // namespace

class HybridIconifyControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void iconifiesThroughThePlatformAndRecordsTheFrames();
    void rejectsDuplicateEmptyAndInvalidRequests();
    void keepsNoRecordWhenThePlatformRefuses();
    void restoreShowsTheWindowAndReturnsTheRecordOnce();
    void restoreDropsTheRecordEvenWhenShowFails();
    void relocateMovesTheChipAndTheRestoreFrameTogetherWithinBounds();
    void revealedIsADeliberateUnrollThatForgetsTheWindow();
    void reapplyReHidesOnlyIconifiedWindows();
    void closedWindowsAreForgottenWithoutPlatformCalls();
    void restoreAllShowsEveryWindowInOrderAndForgetsThem();
};

void HybridIconifyControllerTests::iconifiesThroughThePlatformAndRecordsTheFrames()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QString error;
    QVERIFY2(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, true, &error),
             qPrintable(error));
    QVERIFY(controller.isIconified(QStringLiteral("a")));
    QCOMPARE(controller.count(), 1);
    QCOMPARE(platform.hidden, QStringList{QStringLiteral("a")});
    const auto record = controller.record(QStringLiteral("a"));
    QVERIFY(record.has_value());
    QCOMPARE(record->restoreFrame, WindowFrame);
    QCOMPARE(record->chipFrame, ChipFrame);
    QVERIFY(record->wasActive);
    QVERIFY(controller.iconify(QStringLiteral("b"), WindowFrame.translated(50, 50),
                               ChipFrame.translated(50, 50), false));
    QCOMPARE(controller.iconifiedWindowIds(),
             QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    QVERIFY(!controller.record(QStringLiteral("zzz")).has_value());
}

void HybridIconifyControllerTests::rejectsDuplicateEmptyAndInvalidRequests()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QString error;
    QVERIFY(!controller.iconify({}, WindowFrame, ChipFrame, false, &error));
    QVERIFY(error.contains(QStringLiteral("names no window")));
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    QVERIFY(!controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false, &error));
    QVERIFY(error.contains(QStringLiteral("already iconified")));
    QVERIFY(!controller.iconify(QStringLiteral("b"), QRectF(), ChipFrame, false, &error));
    QVERIFY(error.contains(QStringLiteral("invalid")));
    QVERIFY(!controller.iconify(QStringLiteral("b"), WindowFrame,
                                QRectF(0.0, 0.0, std::nan(""), 1.0), false, &error));
    QCOMPARE(platform.calls, QStringList{QStringLiteral("hide:a")});
    QCOMPARE(controller.count(), 1);
}

void HybridIconifyControllerTests::keepsNoRecordWhenThePlatformRefuses()
{
    FakeIconifyPlatform platform;
    platform.failingHide.append(QStringLiteral("a"));
    HybridIconifyController controller(platform);
    QString error;
    QVERIFY(!controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false, &error));
    QVERIFY(error.contains(QStringLiteral("sentinel hide failure")));
    QVERIFY(!controller.isIconified(QStringLiteral("a")));
    QCOMPARE(controller.count(), 0);
    QVERIFY(platform.hidden.isEmpty());
}

void HybridIconifyControllerTests::restoreShowsTheWindowAndReturnsTheRecordOnce()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, true));
    QString error;
    const auto record = controller.restore(QStringLiteral("a"), &error);
    QVERIFY2(record.has_value(), qPrintable(error));
    QCOMPARE(record->restoreFrame, WindowFrame);
    QVERIFY(record->wasActive);
    QVERIFY(!controller.isIconified(QStringLiteral("a")));
    QVERIFY(platform.hidden.isEmpty());
    QVERIFY(!controller.restore(QStringLiteral("a"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("not iconified")));
    QCOMPARE(platform.calls, QStringList({QStringLiteral("hide:a"), QStringLiteral("show:a")}));
}

void HybridIconifyControllerTests::restoreDropsTheRecordEvenWhenShowFails()
{
    FakeIconifyPlatform platform;
    platform.failingShow.append(QStringLiteral("a"));
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    QString error;
    QVERIFY(!controller.restore(QStringLiteral("a"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("sentinel show failure")));
    // AGENT-GUARD: a window that closed mid-restore must never stay recorded,
    // or shutdown and later chip publication would keep targeting it.
    QVERIFY(!controller.isIconified(QStringLiteral("a")));
}

void HybridIconifyControllerTests::relocateMovesTheChipAndTheRestoreFrameTogetherWithinBounds()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    QString error;
    QVERIFY2(controller.relocateChip(QStringLiteral("a"), QPointF(300.0, 420.0), Output, &error),
             qPrintable(error));
    auto record = controller.record(QStringLiteral("a"));
    QCOMPARE(record->chipFrame, QRectF(300.0, 420.0, 48.0, 48.0));
    QCOMPARE(record->restoreFrame, WindowFrame.translated(200.0, 300.0));

    // Clamped into the output: the restore frame moves only by the clamped delta.
    QVERIFY(controller.relocateChip(QStringLiteral("a"), QPointF(5000.0, -40.0), Output));
    record = controller.record(QStringLiteral("a"));
    QCOMPARE(record->chipFrame, QRectF(1872.0, 0.0, 48.0, 48.0));
    QCOMPARE(record->restoreFrame, WindowFrame.translated(1772.0, -120.0));

    // Invalid bounds disable clamping entirely.
    QVERIFY(controller.relocateChip(QStringLiteral("a"), QPointF(-10.0, -10.0), QRectF()));
    QCOMPARE(controller.record(QStringLiteral("a"))->chipFrame.topLeft(), QPointF(-10.0, -10.0));

    QVERIFY(!controller.relocateChip(QStringLiteral("a"), QPointF(std::nan(""), 0.0), Output,
                                     &error));
    QVERIFY(error.contains(QStringLiteral("not finite")));
    QVERIFY(!controller.relocateChip(QStringLiteral("b"), QPointF(0.0, 0.0), Output, &error));
    QVERIFY(error.contains(QStringLiteral("not iconified")));
    QCOMPARE(platform.calls, QStringList{QStringLiteral("hide:a")});
}

void HybridIconifyControllerTests::revealedIsADeliberateUnrollThatForgetsTheWindow()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    QVERIFY(!controller.revealed(QStringLiteral("stranger")).has_value());
    const auto record = controller.revealed(QStringLiteral("a"));
    QVERIFY(record.has_value());
    QCOMPARE(record->restoreFrame, WindowFrame);
    QVERIFY(!controller.isIconified(QStringLiteral("a")));
    // The content treatment is undone through the platform, never re-applied.
    QCOMPARE(platform.calls, QStringList({QStringLiteral("hide:a"), QStringLiteral("show:a")}));
    QVERIFY(!controller.revealed(QStringLiteral("a")).has_value());
}

void HybridIconifyControllerTests::reapplyReHidesOnlyIconifiedWindows()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    platform.hidden.clear(); // a scene restart lost the item treatment
    QString error;
    QVERIFY(controller.reapply(QStringLiteral("a"), &error));
    QCOMPARE(platform.hidden, QStringList{QStringLiteral("a")});
    QVERIFY(!controller.reapply(QStringLiteral("b"), &error));
    QVERIFY(error.contains(QStringLiteral("not iconified")));
    platform.failingHide.append(QStringLiteral("a"));
    QVERIFY(!controller.reapply(QStringLiteral("a"), &error));
    QVERIFY(controller.isIconified(QStringLiteral("a")));
}

void HybridIconifyControllerTests::closedWindowsAreForgottenWithoutPlatformCalls()
{
    FakeIconifyPlatform platform;
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, false));
    QVERIFY(controller.iconify(QStringLiteral("b"), WindowFrame, ChipFrame, false));
    controller.windowClosed(QStringLiteral("a"));
    controller.windowClosed(QStringLiteral("never"));
    QCOMPARE(controller.iconifiedWindowIds(), QStringList{QStringLiteral("b")});
    QCOMPARE(platform.calls, QStringList({QStringLiteral("hide:a"), QStringLiteral("hide:b")}));
}

void HybridIconifyControllerTests::restoreAllShowsEveryWindowInOrderAndForgetsThem()
{
    FakeIconifyPlatform platform;
    platform.failingShow.append(QStringLiteral("b"));
    HybridIconifyController controller(platform);
    QVERIFY(controller.iconify(QStringLiteral("a"), WindowFrame, ChipFrame, true));
    QVERIFY(controller.iconify(QStringLiteral("b"), WindowFrame, ChipFrame, false));
    QVERIFY(controller.iconify(QStringLiteral("c"), WindowFrame, ChipFrame, false));
    QString error;
    const auto records = controller.restoreAll(&error);
    QCOMPARE(records.size(), 3);
    QCOMPARE(records.at(0).windowId, QStringLiteral("a"));
    QCOMPARE(records.at(2).windowId, QStringLiteral("c"));
    QVERIFY(error.contains(QStringLiteral("sentinel show failure for b")));
    QCOMPARE(controller.count(), 0);
    QCOMPARE(platform.calls.mid(3),
             QStringList({QStringLiteral("show:a"), QStringLiteral("show:b"),
                          QStringLiteral("show:c")}));
    QVERIFY(controller.restoreAll().isEmpty());
}

QTEST_APPLESS_MAIN(HybridIconifyControllerTests)
#include "tst_hybridiconifycontroller.moc"
