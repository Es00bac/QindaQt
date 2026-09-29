// SPDX-License-Identifier: GPL-3.0-or-later
// The capture sequence: hide, delay, capture, and the region overlay's
// frozen workspace, against a fake port (no KWin).
#include "capture_flow.h"
#include "fake_capture_port.h"
#include "frame_store.h"

#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Screenshot;

namespace {

QImage filled(QSize size, QColor color)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    return image;
}

QList<ScreenGeometry> twoScreens()
{
    return {{QStringLiteral("DP-1"), QRect(0, 0, 100, 50)},
            {QStringLiteral("HDMI-A-1"), QRect(100, 0, 100, 50)}};
}

} // namespace

class CaptureFlowTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void screenModesCaptureAfterHiding();
    void delayCountsDownBeforeCapturing();
    void regionFreezesTheWorkspaceAndCrops();
    void regionCancelAndEmptySelection();
    void kwinCancellationAndFailureEndTheFlow();
};

void CaptureFlowTest::screenModesCaptureAfterHiding()
{
    FakeCapturePort port;
    FrameStore frames;
    CaptureFlow flow(port, frames, &twoScreens);
    flow.setSettleMilliseconds(0);
    flow.setMode(QStringLiteral("all-screens"));
    QSignalSpy hide(&flow, &CaptureFlow::hideRequested);
    QSignalSpy captured(&flow, &CaptureFlow::captured);
    QVERIFY(flow.start());
    QCOMPARE(hide.count(), 1);
    QVERIFY(!flow.start()); // one flow at a time
    QTRY_COMPARE(port.calls.size(), 1);
    QCOMPARE(flow.phase(), QStringLiteral("capturing"));
    QCOMPARE(port.calls.first().method, QStringLiteral("CaptureWorkspace"));
    port.answerImage(filled({200, 50}, Qt::green));
    QCOMPARE(captured.count(), 1);
    QCOMPARE(captured.first().at(1).toString(), QStringLiteral("all-screens"));
    QCOMPARE(captured.first().at(0).value<QImage>().size(), QSize(200, 50));
    QCOMPARE(flow.phase(), QStringLiteral("idle"));
}

void CaptureFlowTest::delayCountsDownBeforeCapturing()
{
    FakeCapturePort port;
    FrameStore frames;
    CaptureFlow flow(port, frames, &twoScreens);
    flow.setMode(QStringLiteral("active-window"));
    flow.setDelaySeconds(3);
    flow.setDelaySeconds(61); // out of range: ignored
    QCOMPARE(flow.delaySeconds(), 3);
    QVERIFY(flow.start());
    QCOMPARE(flow.phase(), QStringLiteral("waiting"));
    QCOMPARE(flow.countdown(), 3);
    QVERIFY(port.calls.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(port.calls.size(), 1, 5000);
    QCOMPARE(flow.countdown(), 0);
    QCOMPARE(port.calls.first().method, QStringLiteral("CaptureActiveWindow"));
    flow.cancel();
    QCOMPARE(port.cancels, 1);
    QCOMPARE(flow.phase(), QStringLiteral("idle"));
}

void CaptureFlowTest::regionFreezesTheWorkspaceAndCrops()
{
    FakeCapturePort port;
    FrameStore frames;
    CaptureFlow flow(port, frames, &twoScreens);
    flow.setSettleMilliseconds(0);
    QCOMPARE(flow.mode(), QStringLiteral("region"));
    QSignalSpy captured(&flow, &CaptureFlow::captured);
    QVERIFY(flow.start());
    QTRY_COMPARE(port.calls.size(), 1);
    // A scale-2 workspace: left output red, right output blue.
    QImage workspace = filled({400, 100}, Qt::red);
    for (int y = 0; y < 100; ++y)
        for (int x = 200; x < 400; ++x)
            workspace.setPixelColor(x, y, Qt::blue);
    port.answerImage(workspace, 2.0);
    QCOMPARE(flow.phase(), QStringLiteral("selecting"));
    QCOMPARE(captured.count(), 0);
    QCOMPARE(flow.workspaceBounds(), QRect(0, 0, 200, 50));
    QCOMPARE(flow.overlayScreens().size(), 2);
    const QVariantMap right = flow.overlayScreens().at(1).toMap();
    QCOMPARE(right.value(QStringLiteral("x")).toInt(), 100);
    QVERIFY(right.value(QStringLiteral("source")).toString().startsWith(QStringLiteral("image://capture/screen/")));
    // Each overlay shows exactly its own output's pixels.
    QCOMPARE(frames.screenImage(1).size(), QSize(200, 100));
    QCOMPARE(frames.screenImage(1).pixelColor(0, 0), QColor(Qt::blue));
    CaptureImageProvider provider(frames);
    QSize size;
    QCOMPARE(provider.requestImage(QStringLiteral("screen/9/0"), &size, {}).pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(size, QSize(200, 100));

    const QRect selection = flow.selectionFromPoints(QPointF(90, 10), QPointF(110, 30));
    QCOMPARE(selection, QRect(90, 10, 20, 20));
    QCOMPARE(flow.pixelSize(selection), QSize(40, 40));
    flow.confirmRegion(selection);
    QCOMPARE(captured.count(), 1);
    const QImage crop = captured.first().at(0).value<QImage>();
    QCOMPARE(crop.size(), QSize(40, 40));
    QCOMPARE(crop.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(crop.pixelColor(39, 39), QColor(Qt::blue));
    // The frozen desktop is dropped as soon as the region is taken.
    QVERIFY(frames.workspace().image.isNull());
    QCOMPARE(flow.phase(), QStringLiteral("idle"));
}

void CaptureFlowTest::regionCancelAndEmptySelection()
{
    FakeCapturePort port;
    FrameStore frames;
    CaptureFlow flow(port, frames, &twoScreens);
    flow.setSettleMilliseconds(0);
    QSignalSpy cancelled(&flow, &CaptureFlow::cancelled);
    QSignalSpy rejected(&flow, &CaptureFlow::selectionRejected);
    QSignalSpy captured(&flow, &CaptureFlow::captured);
    QVERIFY(flow.start());
    QTRY_COMPARE(port.calls.size(), 1);
    port.answerImage(filled({200, 50}, Qt::red));
    QCOMPARE(flow.phase(), QStringLiteral("selecting"));
    flow.confirmRegion(QRectF());
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(flow.phase(), QStringLiteral("selecting"));
    // Keyboard editing starts in the middle of the chosen output.
    QCOMPARE(flow.keyboardSelection(1), QRect(125, 12, 50, 25));
    QCOMPARE(flow.nudgeSelection(QRectF(125, 12, 50, 25), 1000, 0, false), QRect(150, 12, 50, 25));
    flow.cancel();
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(captured.count(), 0);
    QVERIFY(flow.overlayScreens().isEmpty());
    QVERIFY(frames.workspace().image.isNull());
}

void CaptureFlowTest::kwinCancellationAndFailureEndTheFlow()
{
    FakeCapturePort port;
    FrameStore frames;
    CaptureFlow flow(port, frames, &twoScreens);
    flow.setSettleMilliseconds(0);
    flow.setMode(QStringLiteral("window-under-pointer"));
    QSignalSpy cancelled(&flow, &CaptureFlow::cancelled);
    QSignalSpy failed(&flow, &CaptureFlow::failed);
    QVERIFY(flow.start());
    QTRY_COMPARE(port.calls.size(), 1);
    DecodedCapture escape;
    escape.cancelled = true;
    port.answer(escape);
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(failed.count(), 0);
    QVERIFY(flow.start());
    QTRY_COMPARE(port.calls.size(), 2);
    DecodedCapture denied;
    denied.error = QStringLiteral("KWin did not allow this screenshot.");
    port.answer(denied);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toString(), denied.error);
    QCOMPARE(flow.phase(), QStringLiteral("idle"));
}

QTEST_MAIN(CaptureFlowTest)
#include "tst_capture_flow.moc"
