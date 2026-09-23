// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/audio_applet_qml_fixture.h"

#include <QWheelEvent>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

namespace {
// Deliver to the popup's own QQuickWindow; the summary panel is in a
// different window and cannot receive the slider's wheel events.
bool sendWheel(QQuickItem *item, QPoint pixels, QPoint angles,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    auto *window = item->window();
    if (window == nullptr) return false;
    const QPointF point = item->mapToScene(
        QPointF(item->width() / 2.0, item->height() / 2.0));
    QWheelEvent event(point, window->mapToGlobal(point.toPoint()), pixels,
                      angles, Qt::NoButton, modifiers, Qt::NoScrollPhase,
                      false);
    event.ignore();
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
    return event.isAccepted();
}

} // namespace

class AudioAppletWheelQmlTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void wheelAccumulatesOnStreamAndDeviceThroughPendingReadback();
};

void AudioAppletWheelQmlTests::wheelAccumulatesOnStreamAndDeviceThroughPendingReadback()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), clientSnapshot());
    QCOMPARE(client.state(), Audio::ClientState::Ready);

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller,
                        {QStringLiteral("audio-volume-medium")}, &error),
             qPrintable(error));
    QQuickWindow window;
    window.setGeometry(0, 0, 420, 640);
    QQuickItem *root = harness.root();
    QVERIFY(root != nullptr);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());
    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("audioAppletSummary"));
    QVERIFY(summary != nullptr);
    summary->forceActiveFocus();
    QQuickItem *popupContent = openPopupContent(root, &window);
    QVERIFY(popupContent != nullptr);
    const auto streams = visualItemsNamed(
        popupContent, QStringLiteral("audioStreamVolume"));
    const auto devices = visualItemsNamed(
        popupContent, QStringLiteral("audioDeviceVolume"));
    QCOMPARE(streams.size(), 1);
    QCOMPARE(devices.size(), 2);
    QQuickItem *stream = streams.constFirst();
    QQuickItem *device = devices.constFirst();
    QCOMPARE(stream->property("value").toDouble(), 0.75);

    sendWheel(stream, {}, {});
    sendWheel(stream, {}, QPoint(120, 0));
    sendWheel(stream, {}, QPoint(0, 120), Qt::ControlModifier);
    QCOMPARE(transport.operations.size(), 0);
    for (int i = 0; i < 3; ++i)
        sendWheel(stream, {}, QPoint(0, 30));
    QCOMPARE(transport.operations.size(), 0);
    sendWheel(stream, {}, QPoint(0, 30));
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.primary.serial, 30ULL);
    QCOMPARE(transport.operations.constFirst().request.volume, 0.76);
    QVERIFY(stream->isEnabled());
    sendWheel(stream, {}, QPoint(0, 240));
    sendWheel(stream, QPoint(0, 40), {});
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(qAbs(stream->property("value").toDouble() - 0.79) < 1e-8);

    transport.finish(transport.operations.constFirst(),
                     successfulResult(transport.operations.constFirst(), 3));
    QTRY_COMPARE(transport.operations.size(), 2);
    QCOMPARE(transport.operations.constLast().request.volume, 0.79);
    transport.finish(transport.operations.constLast(),
                     successfulResult(transport.operations.constLast(), 3));
    QTRY_COMPARE(countPendingRows(controller), 0);
    // Success alone is not the authoritative echo; the wheel intent remains
    // visible until a newer snapshot either confirms or replaces it.
    QVERIFY(qAbs(stream->property("value").toDouble() - 0.79) < 1e-8);

    sendWheel(device, {}, QPoint(0, 120));
    QTRY_COMPARE(transport.operations.size(), 3);
    QCOMPARE(transport.operations.constLast().request.primary.serial, 10ULL);
    QCOMPARE(transport.operations.constLast().request.volume, 0.51);
    device->setProperty("value", 1.0);
    const qsizetype beforeBound = transport.operations.size();
    sendWheel(device, {}, QPoint(0, 120));
    QCOMPARE(transport.operations.size(), beforeBound);

    Audio::Snapshot denied = clientSnapshot(11, 4);
    denied.streams[0].canSetVolume = false;
    transport.invalidate(kOwner, 11, 4);
    transport.reply(transport.fetches.constLast(), denied);
    QTRY_VERIFY(!stream->isEnabled());
    const qsizetype before = transport.operations.size();
    sendWheel(stream, {}, QPoint(0, 120));
    QCOMPARE(transport.operations.size(), before);
}

QTEST_MAIN(AudioAppletWheelQmlTests)
#include "tst_audio_applet_wheel.moc"
