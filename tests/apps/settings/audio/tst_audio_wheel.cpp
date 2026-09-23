// SPDX-License-Identifier: GPL-3.0-or-later

// Real QWheelEvents through the production Audio page. These rows cover the
// wheel admission contract separately from page layout and console geometry.
#include "audio_page_test_support.h"
#include "audio_settings_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtGui/QWheelEvent>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsAudio::AudioSettingsModel;
using QindaQt::Apps::SettingsAudio::TestSupport::FakeAudioTransport;
using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Audio::AudioClient;
using QindaQt::Audio::OperationKind;
using QindaQt::Audio::OperationStatus;
using QindaQt::Audio::Snapshot;
using QindaQt::Apps::SettingsAudio::TestSupport::audioResult;
using QindaQt::Apps::SettingsAudio::TestSupport::readyAudioSnapshot;
using QindaQt::Apps::SettingsAudio::TestSupport::createAudioPage;
using QindaQt::Apps::SettingsAudio::TestSupport::findItem;
using QindaQt::Apps::SettingsAudio::TestSupport::prepareAudioPageEngine;

namespace {

std::pair<std::unique_ptr<QObject>, QQuickItem *>
createRealAudioPage(QQuickView &view, AudioSettingsModel &model)
{
    QQmlComponent component(view.engine());
    component.loadUrl(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_AUDIO_PAGE_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("audioSettings"),
         QVariant::fromValue(static_cast<QObject *>(&model))},
    });
    if (object == nullptr) {
        qWarning().noquote() << component.errorString();
        return {};
    }
    std::unique_ptr<QObject> guard(object);
    auto *page = qobject_cast<QQuickItem *>(object);
    if (page == nullptr) return {};
    QindaQt::Apps::SettingsAudio::TestSupport::attach(view, *page,
                                                        QSize(1100, 900));
    return {std::move(guard), page};
}

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
    void realModelPendingWheelKeepsLatest_data();
    void realModelPendingWheelKeepsLatest();
    void realModelDropsQueuedWheelWithoutReplay_data();
    void realModelDropsQueuedWheelWithoutReplay();
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

void AudioWheelTest::realModelPendingWheelKeepsLatest_data()
{
    QTest::addColumn<bool>("stream");
    QTest::newRow("output-device") << false;
    QTest::newRow("application-stream") << true;
}

void AudioWheelTest::realModelPendingWheelKeepsLatest()
{
    QFETCH(bool, stream);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    AudioSettingsModel model(client);
    client.setRequestTimeout(2000);
    client.start();
    transport.announceOwner(QStringLiteral(":1.7"));
    QTRY_COMPARE(transport.fetches.size(), 1);
    transport.reply(transport.fetches.constLast(), readyAudioSnapshot());
    QTRY_VERIFY(model.ready());

    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    auto [guard, page] = createRealAudioPage(view, model);
    QVERIFY(page != nullptr);
    auto *slider = findItem(page, stream
        ? QStringLiteral("audioStreamVolume_30")
        : QStringLiteral("audioOutputVolume_10"));
    QVERIFY(slider != nullptr);
    reveal(page, slider);
    const double initial = stream ? 0.75 : 0.5;
    QVERIFY(near(slider->property("value").toDouble(), initial));

    sendWheel(slider, {}, QPoint(0, 120));
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constLast().request.kind,
             OperationKind::SetVolume);
    QCOMPARE(transport.operations.constLast().request.primary.serial,
             stream ? 30ULL : 10ULL);
    QVERIFY(near(transport.operations.constLast().request.volume,
                 initial + 0.01));
    sendWheel(slider, {}, QPoint(0, 120));
    sendWheel(slider, {}, QPoint(0, 240));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(near(slider->property("value").toDouble(), initial + 0.04));
    const QVariantMap pendingRow = stream ? model.streams().first().toMap()
        : model.outputDevices().first().toMap();
    QCOMPARE(pendingRow.value(QStringLiteral("volumePercent")).toInt(),
             int(initial * 100));
    QCOMPARE(pendingRow.value(QStringLiteral("volumeDisplayPercent")).toInt(),
             int(initial * 100 + 4));

    transport.finish(transport.operations.constFirst(),
        audioResult(OperationKind::SetVolume, OperationStatus::Succeeded,
                    11, 2));
    QTRY_COMPARE(transport.fetches.size(), 2);
    // A further detent after success but before its authoritative readback
    // still replaces the queued latest; it must not dispatch against stale
    // snapshot truth.
    sendWheel(slider, {}, QPoint(0, 120));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(near(slider->property("value").toDouble(), initial + 0.05));
    Snapshot firstEcho = readyAudioSnapshot(11, 3);
    if (stream) {
        firstEcho.streams[0].volume = initial + 0.01;
        firstEcho.streams[0].channelVolumes = {initial + 0.01,
                                               initial + 0.01};
    } else {
        firstEcho.outputs[0].volume = initial + 0.01;
        firstEcho.outputs[0].channelVolumes = {initial + 0.01,
                                               initial + 0.01};
    }
    transport.reply(transport.fetches.constLast(), firstEcho);
    QTRY_COMPARE(transport.operations.size(), 2);
    QVERIFY(near(transport.operations.constLast().request.volume,
                 initial + 0.05));
    QVERIFY(near(slider->property("value").toDouble(), initial + 0.05));

    transport.finish(transport.operations.constLast(),
        audioResult(OperationKind::SetVolume, OperationStatus::Succeeded,
                    11, 3));
    QTRY_COMPARE(transport.fetches.size(), 3);
    Snapshot finalEcho = readyAudioSnapshot(11, 4);
    if (stream) {
        finalEcho.streams[0].volume = initial + 0.05;
        finalEcho.streams[0].channelVolumes = {initial + 0.05,
                                               initial + 0.05};
    } else {
        finalEcho.outputs[0].volume = initial + 0.05;
        finalEcho.outputs[0].channelVolumes = {initial + 0.05,
                                               initial + 0.05};
    }
    transport.reply(transport.fetches.constLast(), finalEcho);
    QTRY_VERIFY(near(slider->property("value").toDouble(), initial + 0.05));
    const QVariantMap confirmedRow = stream ? model.streams().first().toMap()
        : model.outputDevices().first().toMap();
    QCOMPARE(confirmedRow.value(QStringLiteral("volumePercent")).toInt(),
             int(initial * 100 + 5));
    QCOMPARE(confirmedRow.value(QStringLiteral("volumeDisplayPercent")).toInt(),
             int(initial * 100 + 5));
    QTRY_VERIFY(!(stream ? model.streams().first().toMap()
        : model.outputDevices().first().toMap())
        .value(QStringLiteral("pending")).toBool());
}

void AudioWheelTest::realModelDropsQueuedWheelWithoutReplay_data()
{
    QTest::addColumn<bool>("stream");
    QTest::addColumn<QString>("outcome");
    for (const bool stream : {false, true}) {
        for (const QString &outcome : {QStringLiteral("rejected"),
                                       QStringLiteral("uncertain"),
                                       QStringLiteral("owner-loss"),
                                       QStringLiteral("epoch-replace"),
                                       QStringLiteral("capability-loss")}) {
            const QString name = QStringLiteral("%1-%2")
                .arg(stream ? QStringLiteral("stream")
                            : QStringLiteral("device"), outcome);
            QTest::newRow(qPrintable(name)) << stream << outcome;
        }
    }
}

void AudioWheelTest::realModelDropsQueuedWheelWithoutReplay()
{
    QFETCH(bool, stream);
    QFETCH(QString, outcome);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    AudioSettingsModel model(client);
    client.setRequestTimeout(2000);
    client.start();
    transport.announceOwner(QStringLiteral(":1.7"));
    QTRY_COMPARE(transport.fetches.size(), 1);
    transport.reply(transport.fetches.constLast(), readyAudioSnapshot());
    QTRY_VERIFY(model.ready());
    const quint64 serial = stream ? 30 : 10;
    const double initial = stream ? 0.75 : 0.5;
    QVERIFY(stream ? model.setStreamVolume(serial, initial + 0.01)
                   : model.setDeviceVolume(serial, initial + 0.01));
    QVERIFY(stream ? model.setStreamVolume(serial, initial + 0.02)
                   : model.setDeviceVolume(serial, initial + 0.02));
    QCOMPARE(transport.operations.size(), 1);
    const auto first = transport.operations.constFirst();

    if (outcome == QStringLiteral("rejected")
        || outcome == QStringLiteral("uncertain")) {
        const OperationStatus status = outcome == QStringLiteral("rejected")
            ? OperationStatus::Rejected : OperationStatus::Uncertain;
        transport.finish(first, audioResult(OperationKind::SetVolume,
                                            status, 11, 2,
                                            QStringLiteral("stale-handle")));
        QCoreApplication::processEvents();
        const QVariantMap row = stream ? model.streams().first().toMap()
            : model.outputDevices().first().toMap();
        QTRY_VERIFY(!(stream ? model.streams().first().toMap()
            : model.outputDevices().first().toMap())
            .value(QStringLiteral("pending")).toBool());
        QCOMPARE(row.value(QStringLiteral("volumePercent")).toInt(),
                 int(initial * 100));
        QCOMPARE((stream ? model.streams().first().toMap()
            : model.outputDevices().first().toMap())
            .value(QStringLiteral("volumeDisplayPercent")).toInt(),
                 int(initial * 100));
    } else if (outcome == QStringLiteral("owner-loss")) {
        transport.announceOwner(QString());
        QTRY_VERIFY(model.outputDevices().isEmpty());
        transport.announceOwner(QStringLiteral(":1.42"));
        QTRY_COMPARE(transport.fetches.size(), 2);
        transport.reply(transport.fetches.constLast(),
                        readyAudioSnapshot(12, 1));
        QTRY_VERIFY(model.ready());
    } else {
        Snapshot replacement = outcome == QStringLiteral("epoch-replace")
            ? readyAudioSnapshot(12, 1) : readyAudioSnapshot(11, 3);
        if (outcome == QStringLiteral("capability-loss")) {
            replacement.capabilities &=
                ~QindaQt::Audio::Capabilities(QindaQt::Audio::Capability::SetVolume);
            for (auto &device : replacement.outputs) device.canSetVolume = false;
            for (auto &device : replacement.inputs) device.canSetVolume = false;
            for (auto &item : replacement.streams) item.canSetVolume = false;
        }
        transport.invalidate(QStringLiteral(":1.7"), replacement.epoch,
                             replacement.revision);
        QTRY_COMPARE(transport.fetches.size(), 2);
        transport.reply(transport.fetches.constLast(), replacement);
        QCoreApplication::processEvents();
        if (outcome == QStringLiteral("capability-loss")) {
            transport.finish(first,
                audioResult(OperationKind::SetVolume, OperationStatus::Succeeded,
                            11, 2));
            QCoreApplication::processEvents();
        }
    }
    QCOMPARE(transport.operations.size(), 1);
    const QVariantMap finalRow = stream ? model.streams().first().toMap()
        : model.outputDevices().first().toMap();
    QCOMPARE(finalRow.value(QStringLiteral("volumeDisplayPercent")).toInt(),
             finalRow.value(QStringLiteral("volumePercent")).toInt());
}

QTEST_MAIN(AudioWheelTest)
#include "tst_audio_wheel.moc"
