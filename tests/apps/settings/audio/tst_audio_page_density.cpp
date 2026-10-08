// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_page_test_support.h"
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
};
QTEST_MAIN(AudioPageDensityTest)
#include "tst_audio_page_density.moc"
