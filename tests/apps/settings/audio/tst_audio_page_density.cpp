// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_page_test_support.h"
#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtTest>
#include <QtQml/QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Apps::SettingsAudio::TestSupport;

class AudioPageDensityTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void compactDeviceCardsLeaveRoomForControls_data() {
    QTest::addColumn<QSize>("size");
    QTest::newRow("wide") << QSize(900, 720);
    QTest::newRow("compact") << QSize(420, 320);
  }

  void compactDeviceCardsLeaveRoomForControls() {
    QFETCH(QSize, size);
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, size);
    QVERIFY(page != nullptr);
    auto *first = findItem(page, QStringLiteral("audioOutputVolume_10"));
    auto *second = findItem(page, QStringLiteral("audioOutputVolume_12"));
    auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(viewport != nullptr);
    // The already-default device intentionally hides its set-default action.
    // Measure always-visible volume controls rather than that hidden action.
    QTRY_VERIFY(first->isVisible());
    QTRY_VERIFY(second->isVisible());
    QVERIFY(first->isEnabled());
    QVERIFY(second->isEnabled());
    // Wait for both laid-out controls before comparing their spacing.
    QTRY_VERIFY(first->height() >= 22);
    QTRY_VERIFY(second->height() >= 22);
    // Observe laid-out action positions, not the padding implementation.
    QTRY_VERIFY(second->mapToItem(viewport, 0, 0).y()
                > first->mapToItem(viewport, 0, 0).y());
    const auto pitch = second->mapToItem(viewport, 0, 0).y()
                       - first->mapToItem(viewport, 0, 0).y();
    QVERIFY2(pitch <= 64,
             qPrintable(QStringLiteral("Device pitch: %1 logical pixels")
                            .arg(pitch)));
    auto *input = findItem(page, QStringLiteral("audioInputVolume_20"));
    auto *setDefault = findItem(page, QStringLiteral("audioOutputDefault_12"));
    QVERIFY(input != nullptr);
    QVERIFY(input->isVisible());
    QVERIFY(input->isEnabled());
    QVERIFY(setDefault != nullptr);
    QVERIFY(setDefault->isVisible());
    QVERIFY(setDefault->isEnabled());
    QTRY_VERIFY(input->height() >= 22);
    // A compact window must expose both output faders and the first input
    // fader together, without relying on scrolling or hiding their actions.
    QCOMPARE(viewport->property("contentY").toReal(), 0.0);
    QTRY_VERIFY(input->mapToItem(viewport, 0, input->height()).y()
                <= viewport->height());
    QVERIFY(findItem(page, QStringLiteral("audioOutputVolume_10")) != nullptr);
    QVERIFY(findItem(page, QStringLiteral("audioInputMute_20")) != nullptr);
    const auto path = qEnvironmentVariable("QINDAQT_AUDIO_SETTINGS_CAPTURE_PATH");
    if (!path.isEmpty())
      QVERIFY(view.grabWindow().save(path));
  }

  void detailsRemainAccessibleAcrossSnapshots() {
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *details = findItem(page, QStringLiteral("audioOutputDetails_10"));
    auto *field = findItem(page, QStringLiteral("audioOutputLatency_10"));
    QVERIFY(details != nullptr);
    QVERIFY(field != nullptr);
    QVERIFY(!field->isVisible());
    auto *accessible = QAccessible::queryAccessibleInterface(details);
    QVERIFY(accessible != nullptr);
    QVERIFY(accessible->text(QAccessible::Name).contains(QStringLiteral("Desk Speakers")));
    details->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_VERIFY(field->isVisible());
    auto *channels = findItem(page, QStringLiteral("audioChannelsToggle_10"));
    QVERIFY(channels != nullptr);
    channels->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_VERIFY(findItem(page, QStringLiteral("audioChannelVolume_10_0")) != nullptr);

    auto row = model.outputDevices.at(0).toMap();
    row[QStringLiteral("volumePercent")] = 77;
    model.outputDevices[0] = row;
    Q_EMIT model.viewChanged();
    QCoreApplication::processEvents();
    QCOMPARE(findItem(page, QStringLiteral("audioOutputDetails_10")), details);
    QVERIFY(field->isVisible());
    QVERIFY(details->property("checked").toBool());
    QVERIFY(findItem(page, QStringLiteral("audioChannelVolume_10_0")) != nullptr);

    // A row index can outlive a disappeared device. Its replacement starts
    // compact; no old expanded channel editor or write follows that index.
    row[QStringLiteral("serial")] = qulonglong(30);
    model.outputDevices[0] = row;
    Q_EMIT model.viewChanged();
    QCoreApplication::processEvents();
    QCOMPARE(findItem(page, QStringLiteral("audioOutputDetails_30")), details);
    QTRY_VERIFY(!field->isVisible());
    QVERIFY(!details->property("checked").toBool());
    QVERIFY(findItem(page, QStringLiteral("audioChannelVolume_30_0")) == nullptr);
    QCOMPARE(model.latencyCount, 0);
    QCOMPARE(model.channelSerial, qulonglong(0));
  }
};
QTEST_MAIN(AudioPageDensityTest)
#include "tst_audio_page_density.moc"
