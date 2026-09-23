// SPDX-License-Identifier: GPL-3.0-or-later

// Real QWheelEvents through the production Audio page. These rows cover the
// wheel admission contract separately from page layout and console geometry.
#include "audio_page_test_support.h"

#include <QtGui/QWheelEvent>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsAudio::TestSupport::createAudioPage;
using QindaQt::Apps::SettingsAudio::TestSupport::findItem;
using QindaQt::Apps::SettingsAudio::TestSupport::prepareAudioPageEngine;

namespace {

QQuickItem *viewportFor(QQuickItem *page)
{
    return findItem(page, QStringLiteral("audioFormViewport"));
}

void reveal(QQuickItem *page, QQuickItem *item)
{
    auto *viewport = viewportFor(page);
    Q_ASSERT(viewport != nullptr);
    const qreal current = viewport->property("contentY").toReal();
    const qreal viewportCenter =
        viewport->mapToScene(QPointF(0, viewport->height() / 2.0)).y();
    const qreal itemCenter =
        item->mapToScene(QPointF(0, item->height() / 2.0)).y();
    const qreal maxY = qMax(0.0,
        viewport->property("contentHeight").toReal() - viewport->height());
    viewport->setProperty("contentY",
        qBound(0.0, current + itemCenter - viewportCenter, maxY));
    QCoreApplication::processEvents();
}

bool sendWheel(QQuickItem *item, const QPoint pixelDelta,
               const QPoint angleDelta,
               const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    auto *window = item->window();
    if (window == nullptr) return false;
    const QPointF local = item->mapToScene(
        QPointF(item->width() / 2.0, item->height() / 2.0));
    QWheelEvent event(local, window->mapToGlobal(local.toPoint()), pixelDelta,
                      angleDelta, Qt::NoButton, modifiers,
                      Qt::NoScrollPhase, false);
    event.ignore();
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
    return event.isAccepted();
}

bool near(const double actual, const double expected)
{
    return qAbs(actual - expected) < 1e-8;
}

} // namespace

class AudioWheelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void settingsVolumeAccumulatesAndRestoresTruth();
    void settingsVolumePassesWheelToScrollerAtBoundsAndWhenDisabled();
    void consoleKnobAndFaderAccumulateDetents();
};

void AudioWheelTest::settingsVolumeAccumulatesAndRestoresTruth()
{
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, QSize(1100, 900));
    QVERIFY(page != nullptr);
    auto *slider = findItem(page, QStringLiteral("audioOutputVolume_10"));
    QVERIFY(slider != nullptr);
    reveal(page, slider);
    QCOMPARE(slider->property("value").toDouble(), 0.5);

    sendWheel(slider, {}, {});
    sendWheel(slider, {}, QPoint(120, 0));
    sendWheel(slider, {}, QPoint(0, 120), Qt::ControlModifier);
    QCOMPARE(model.deviceVolumeSerial, qulonglong(0));
    for (int i = 0; i < 3; ++i)
        sendWheel(slider, {}, QPoint(0, 30));
    QCOMPARE(model.deviceVolumeSerial, qulonglong(0));
    sendWheel(slider, {}, QPoint(0, 30));
    QCOMPARE(model.deviceVolumeSerial, qulonglong(10));
    QVERIFY(near(model.deviceVolumeLevel, 0.51));
    QVERIFY(near(slider->property("value").toDouble(), 0.51));

    sendWheel(slider, {}, QPoint(0, 240));
    QVERIFY(near(model.deviceVolumeLevel, 0.53));
    sendWheel(slider, QPoint(0, 40), {});
    QVERIFY(near(model.deviceVolumeLevel, 0.54));

    // The model remains authoritative after a pending wheel burst. A refused
    // write or an external level update must replace local wheel intent.
    auto row = model.outputDevices.first().toMap();
    row.insert(QStringLiteral("volumePercent"), 40);
    model.outputDevices[0] = row;
    Q_EMIT model.viewChanged();
    QTRY_VERIFY(near(slider->property("value").toDouble(), 0.4));
}

void AudioWheelTest::settingsVolumePassesWheelToScrollerAtBoundsAndWhenDisabled()
{
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, QSize(1100, 900));
    QVERIFY(page != nullptr);
    auto *slider = findItem(page, QStringLiteral("audioOutputVolume_10"));
    auto *viewport = viewportFor(page);
    QVERIFY(slider != nullptr);
    QVERIFY(viewport != nullptr);
    reveal(page, slider);

    slider->setProperty("value", 1.0);
    const qreal beforeBound = viewport->property("contentY").toReal();
    QVERIFY(beforeBound > 0);
    sendWheel(slider, {}, QPoint(0, 120));
    QCOMPARE(model.deviceVolumeSerial, qulonglong(0));
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("contentY").toReal() < beforeBound, 1500);

    auto row = model.outputDevices.first().toMap();
    row.insert(QStringLiteral("volumeAvailable"), false);
    model.outputDevices[0] = row;
    Q_EMIT model.viewChanged();
    QTRY_VERIFY(!slider->isEnabled());
    reveal(page, slider);
    const double beforeDisabled = slider->property("value").toDouble();
    QVERIFY(!sendWheel(slider, {}, QPoint(0, 120)));
    QCOMPARE(model.deviceVolumeSerial, qulonglong(0));
    QCOMPARE(slider->property("value").toDouble(), beforeDisabled);
}

void AudioWheelTest::consoleKnobAndFaderAccumulateDetents()
{
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, QSize(1100, 900));
    QVERIFY(page != nullptr);
    auto *knob = findItem(page, QStringLiteral("consoleStripPan_strip.hw.1"));
    auto *fader = findItem(page, QStringLiteral("consoleStripFader_strip.hw.1"));
    QVERIFY(knob != nullptr);
    QVERIFY(fader != nullptr);
    reveal(page, knob);

    sendWheel(knob, {}, {});
    sendWheel(knob, {}, QPoint(120, 0));
    QCOMPARE(model.lastFaderPosition, -1.0);
    for (int i = 0; i < 3; ++i)
        sendWheel(knob, {}, QPoint(0, 30));
    QCOMPARE(model.lastFaderPosition, -1.0);
    sendWheel(knob, {}, QPoint(0, 30));
    QVERIFY(near(model.lastFaderPosition, 0.05));
    sendWheel(knob, {}, QPoint(0, 240));
    QVERIFY(near(model.lastFaderPosition, 0.15));
    sendWheel(knob, QPoint(0, 40), {});
    QVERIFY(near(model.lastFaderPosition, 0.2));
    // This stub deliberately does not echo the edit: the knob must restore
    // the last authoritative value instead of displaying a false setting.
    QTRY_VERIFY_WITH_TIMEOUT(
        near(knob->property("liveValue").toDouble(), 0.0), 1500);

    sendWheel(knob, {}, QPoint(0, 120));
    auto strip = model.consoleStrips.first().toMap();
    strip.insert(QStringLiteral("pan"), 0.05);
    model.consoleStrips[0] = strip;
    Q_EMIT model.viewChanged();
    QTRY_VERIFY(near(knob->property("liveValue").toDouble(), 0.05));
    QTest::qWait(850);
    QVERIFY(near(knob->property("liveValue").toDouble(), 0.05));

    reveal(page, fader);
    model.lastFaderPosition = -1.0;
    const double initial = fader->property("livePosition").toDouble();
    sendWheel(fader, {}, {});
    sendWheel(fader, {}, QPoint(120, 0));
    QCOMPARE(model.lastFaderPosition, -1.0);
    model.busy = true;
    Q_EMIT model.viewChanged();
    for (int i = 0; i < 4; ++i)
        sendWheel(fader, {}, QPoint(0, 30));
    QVERIFY(near(fader->property("livePosition").toDouble(), initial + 0.02));
    sendWheel(fader, {}, QPoint(0, 240));
    sendWheel(fader, QPoint(0, 40), {});
    const double expected = qMin(1.0, initial + 0.08);
    QVERIFY(near(fader->property("livePosition").toDouble(), expected));
    model.busy = false;
    Q_EMIT model.viewChanged();
    QTRY_VERIFY(near(model.lastFaderPosition, expected));
}

QTEST_MAIN(AudioWheelTest)
#include "tst_audio_wheel.moc"
