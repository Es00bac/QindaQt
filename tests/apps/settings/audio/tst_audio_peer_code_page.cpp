// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_page_test_support.h"
#include "src/apps/settings/audio/audio_peer_code.h"

#include <QtCore/QMetaObject>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
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
bool sendWheel(QQuickItem *item, const QPoint angle) {
  auto *window = item->window();
  if (!window) return false;
  const QPointF local = item->mapToScene(QPointF(item->width() / 2,
                                                item->height() / 2));
  QWheelEvent event(local, window->mapToGlobal(local.toPoint()), {}, angle,
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  event.ignore();
  QCoreApplication::sendEvent(window, &event);
  QCoreApplication::processEvents();
  return event.isAccepted();
}
}

class AudioPeerCodePageTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void guidedPeerCodeFlowAtCompactAndWideSizes();
private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubAudioSettingsModel> m_model;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(const QSize size);
};

void AudioPeerCodePageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(prepareAudioPageEngine(*m_view, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
AudioPeerCodePageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubAudioSettingsModel>();
  return createAudioPage(*m_view, *m_model, size);
}

void AudioPeerCodePageTest::guidedPeerCodeFlowAtCompactAndWideSizes() {
  for (const QSize size : {QSize(420, 320), QSize(900, 760)}) {
    auto [guard, page] = createPage(size);
    QVERIFY(page != nullptr);
    auto *tabs = findItem(page, QStringLiteral("audioDestinationTabs"));
    QVERIFY(tabs != nullptr);
    tabs->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(m_view.get(), Qt::Key_Right);
    QTest::keyClick(m_view.get(), Qt::Key_Right);
    QTRY_COMPARE(page->property("activeTab").toInt(), 2);
    auto *sender = findItem(page, QStringLiteral("audioPeerCodeSender"));
    auto *address = findItem(page, QStringLiteral("audioPeerCodeSenderAddress"));
    auto *copyAddress = findItem(page, QStringLiteral("audioPeerCodeCopyAddress"));
    auto *generate = findItem(page, QStringLiteral("audioPeerCodeGenerate"));
    auto *shared = findItem(page, QStringLiteral("audioPeerCodeShare"));
    auto *copy = findItem(page, QStringLiteral("audioPeerCodeCopy"));
    auto *paste = findItem(page, QStringLiteral("audioPeerCodePaste"));
    auto *review = findItem(page, QStringLiteral("audioPeerCodeReview"));
    auto *summary = findItem(page, QStringLiteral("audioPeerCodeReviewSummary"));
    auto *output = findItem(page, QStringLiteral("audioPeerCodeOutput"));
    auto *trust = findItem(page, QStringLiteral("audioPeerCodeTrust"));
    auto *save = findItem(page, QStringLiteral("audioPeerCodeSave"));
    QVERIFY(sender && address && copyAddress && generate && shared && copy && paste && review
            && summary && output && trust && save);
    QCOMPARE(address->property("text").toString(), QStringLiteral("192.0.2.10"));
    copyAddress->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), copyAddress);
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("192.0.2.10"));

    QVERIFY(generate->property("available").toBool());
    generate->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), generate);
    if (size.width() == 420) {
      QTest::keyClick(m_view.get(), Qt::Key_Space);
    } else {
      QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier,
                        generate->mapToScene(QPointF(generate->width() / 2,
                                                       generate->height() / 2)).toPoint());
    }
    QTRY_VERIFY(shared->isVisible());
    QVERIFY(shared->property("text").toString().startsWith(
        QStringLiteral("QINDAQT-AUDIO-1:")));
    QVERIFY(copy->isVisible());
    copy->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), copy);
    if (size.width() == 420) {
      QTest::keyClick(m_view.get(), Qt::Key_Space);
    } else {
      QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier,
                        copy->mapToScene(QPointF(copy->width() / 2,
                                                 copy->height() / 2)).toPoint());
    }
    QCOMPARE(QGuiApplication::clipboard()->text(), shared->property("text").toString());

    const QString remote = QindaQt::Apps::SettingsAudio::encodePeerCode(
        {QStringLiteral("Remote"), QStringLiteral("192.0.2.20"), 6982});
    paste->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), paste);
    QGuiApplication::clipboard()->setText(remote);
    QTest::keyClick(m_view.get(), Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(paste->property("text").toString(), remote);
    auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
    QVERIFY(viewport != nullptr);
    QVERIFY(shared->mapToItem(viewport, QPointF(shared->width(), 0)).x()
            <= viewport->width() + 1);
    QVERIFY(paste->mapToItem(viewport, QPointF(paste->width(), 0)).x()
            <= viewport->width() + 1);
    QTRY_VERIFY_WITH_TIMEOUT(
        paste->mapToItem(viewport, QPointF(paste->width() / 2,
                                          paste->height() / 2)).y()
            <= viewport->height(), 1500);
    if (size.width() == 420) {
      const qreal beforePeerWheel = viewport->property("contentY").toReal();
      QVERIFY(sendWheel(paste, QPoint(0, -120)));
      QTRY_VERIFY_WITH_TIMEOUT(viewport->property("contentY").toReal()
                               > beforePeerWheel, 1500);
    }

    review->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), review);
    if (size.width() == 420) {
      QTest::keyClick(m_view.get(), Qt::Key_Space);
    } else {
      QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier,
                        review->mapToScene(QPointF(review->width() / 2,
                                                   review->height() / 2)).toPoint());
    }
    QTRY_VERIFY(summary->isVisible());
    QVERIFY(summary->property("text").toString().contains(QStringLiteral("192.0.2.20")));
    QVERIFY(output->isVisible());
    QVERIFY(!save->property("available").toBool());
    trust->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QTRY_VERIFY(save->property("available").toBool());
    save->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), save);
    const QPointF local = save->mapToItem(viewport, QPointF(save->width() / 2,
                                                            save->height() / 2));
    QVERIFY(local.y() >= 0.0 && local.y() <= viewport->height());
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QCOMPARE(m_model->savedPeerName, QStringLiteral("Remote"));
    QCOMPARE(m_model->savedPeerHost, QStringLiteral("192.0.2.20"));
    QCOMPARE(m_model->savedPeerPort, 6982);
    QCOMPARE(m_model->savedPeerOutput, QStringLiteral("alsa_output.desk"));
    QVERIFY(!m_model->savedPeerOutgoing);
    QVERIFY(!trust->property("checked").toBool());
    tabs->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(m_view.get(), Qt::Key_Left);
    QTest::keyClick(m_view.get(), Qt::Key_Left);
    QTRY_COMPARE(page->property("activeTab").toInt(), 0);
    auto *volume = findItem(page, QStringLiteral("audioOutputVolume_10"));
    QVERIFY(volume != nullptr);
    volume->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), volume);
    QVERIFY(sendWheel(volume, QPoint(0, 120)));
    QCOMPARE(m_model->deviceVolumeSerial, qulonglong(10));
    m_model->canManagePeerStreams = false;
    m_model->errorText = QStringLiteral("Audio service unavailable");
    Q_EMIT m_model->viewChanged();
    QCoreApplication::processEvents();
    QVERIFY(!generate->property("available").toBool());
    QVERIFY(!review->property("available").toBool());
    QVERIFY(!sender->isEnabled());
    QVERIFY(!paste->isEnabled());
    auto *error = findItem(page, QStringLiteral("audioError"));
    QVERIFY(error && error->isVisible());
    QVERIFY(error->property("text").toString().contains(
        QStringLiteral("unavailable")));


  }
}

QTEST_MAIN(AudioPeerCodePageTest)
#include "tst_audio_peer_code_page.moc"
