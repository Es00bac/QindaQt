// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_page_test_support.h"
#include <QtTest>
#include <QtQml/QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Apps::SettingsAudio::TestSupport;

class AudioPageDensityTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void compactDeviceCardsLeaveRoomForControls() {
    QQuickView view;
    QString error;
    QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
    StubAudioSettingsModel model;
    auto [guard, page] = createAudioPage(view, model, QSize(900, 720));
    QVERIFY(page != nullptr);
    auto *first = findItem(page, QStringLiteral("audioOutputDefault_10"));
    auto *second = findItem(page, QStringLiteral("audioOutputDefault_12"));
    auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(viewport != nullptr);
    // Observe laid-out action positions, not the padding implementation.
    QTRY_VERIFY(second->mapToItem(viewport, 0, 0).y()
                - first->mapToItem(viewport, 0, 0).y() <= 128);
    QVERIFY(first->height() >= 22);
    QVERIFY(second->height() >= 22);
    QVERIFY(findItem(page, QStringLiteral("audioOutputVolume_10")) != nullptr);
    QVERIFY(findItem(page, QStringLiteral("audioInputMute_20")) != nullptr);
    const auto path = qEnvironmentVariable("QINDAQT_AUDIO_SETTINGS_CAPTURE_PATH");
    if (!path.isEmpty())
      QVERIFY(view.grabWindow().save(path));
  }
};
QTEST_MAIN(AudioPageDensityTest)
#include "tst_audio_page_density.moc"
