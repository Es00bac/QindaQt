// SPDX-License-Identifier: GPL-3.0-or-later

// The Settings Audio console's grid tests (ADR-0227): the six shared band
// heights of the strip and bus cards, the fader's fidelity to the one gain
// law (ADR-0171), and the strip/bus control dispatch. The route surface's
// tests live in tst_audio_page.cpp; both suites share
// audio_page_test_support.h.

#include "audio_page_test_support.h"

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsAudio::TestSupport::createAudioPage;
using QindaQt::Apps::SettingsAudio::TestSupport::findItem;
using QindaQt::Apps::SettingsAudio::TestSupport::prepareAudioPageEngine;

class AudioConsolePageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void consoleCardsShareOneGrid();
  void consoleFaderFollowsTheOneGainLaw();
  void consoleStripControlsDispatch();
  void consoleBusControlsDispatch();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubAudioSettingsModel> m_model;

  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(const QSize size);
};

void AudioConsolePageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(prepareAudioPageEngine(*m_view, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
AudioConsolePageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubAudioSettingsModel>();
  return createAudioPage(*m_view, *m_model, size);
}

// The console is read across a row like a real desk, so every card must put
// its meter, fader and pads at the same height as its neighbour's. This row
// exists because they did not: a virtual strip hid the device picker a
// hardware strip carries, which lifted its whole desk band, and buses put
// their picker at the bottom while strips put it at the top. The band heights
// in AudioConsoleStrip.qml and AudioConsoleBus.qml are shared verbatim; this
// fails if they ever drift apart again.
void AudioConsolePageTest::consoleCardsShareOneGrid() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  struct CardUnderTest {
    const char *card;
    const char *fader;
    const char *meter;
  };
  // A hardware strip and a virtual strip (the pair that misaligned), then a
  // physical bus and a virtual bus.
  const CardUnderTest cards[] = {
      {"consoleStrip_strip.hw.1", "consoleStripFader_strip.hw.1",
       "consoleStripMeter_strip.hw.1"},
      {"consoleStrip_strip.virtual.1", "consoleStripFader_strip.virtual.1",
       "consoleStripMeter_strip.virtual.1"},
      {"consoleBus_bus.a1", "consoleBusFader_bus.a1", "consoleBusMeter_bus.a1"},
      {"consoleBus_bus.b1", "consoleBusFader_bus.b1", "consoleBusMeter_bus.b1"},
  };

  qreal sharedFaderOffset = -1.0;
  qreal sharedCardHeight = -1.0;
  for (const CardUnderTest &entry : cards) {
    QQuickItem *card = findItem(page, QString::fromLatin1(entry.card));
    QQuickItem *fader = findItem(page, QString::fromLatin1(entry.fader));
    QQuickItem *meter = findItem(page, QString::fromLatin1(entry.meter));
    QVERIFY2(card != nullptr, entry.card);
    QVERIFY2(fader != nullptr, entry.fader);
    QVERIFY2(meter != nullptr, entry.meter);

    const qreal faderOffset = fader->mapToItem(card, QPointF(0.0, 0.0)).y();
    const qreal meterOffset = meter->mapToItem(card, QPointF(0.0, 0.0)).y();
    QCOMPARE(meterOffset, faderOffset);
    // The desk band height is the one number both files state.
    QCOMPARE(fader->height(), 150.0);
    // The card width is the other: Tk.Flex reads only implicit sizes, so a
    // card that sets `width:` instead of `implicitWidth:` is crushed to zero
    // and every band overflows it. Assert the number, not just equality.
    QCOMPARE(card->width(), 140.0);

    if (sharedFaderOffset < 0.0) {
      sharedFaderOffset = faderOffset;
      sharedCardHeight = card->height();
      continue;
    }
    QCOMPARE(faderOffset, sharedFaderOffset);
    QCOMPARE(card->height(), sharedCardHeight);
  }
  QVERIFY(sharedFaderOffset > 0.0);
}

// The console is operated in decibels, and ADR-0171 admits exactly one
// mapping between the fader's travel and dB: the model's. This row proves the
// QML never re-derives that mapping — the printed scale, the ticks and the
// readout all read the model's own conversion functions.
void AudioConsolePageTest::consoleFaderFollowsTheOneGainLaw() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);
  auto *fader = findItem(page, QStringLiteral("consoleStripFader_strip.hw.1"));
  QVERIFY(fader != nullptr);
  QVERIFY(fader->isEnabled());

  // The readout must be the model's conversion of the live position, never a
  // number the QML computed on its own.
  const qreal live = fader->property("livePosition").toReal();
  const double gainDb =
      QindaQt::Audio::gainDbFromFaderPosition(live);
  const QString expectedReadout =
      (gainDb > 0 ? QStringLiteral("+") : QStringLiteral())
      + QString::number(gainDb, 'f', 1) + QStringLiteral(" dB");
  QCOMPARE(fader->property("readout").toString(), expectedReadout);
  auto *readout = findItem(page, QStringLiteral("consoleFaderReadout"));
  QVERIFY(readout != nullptr);
  QCOMPARE(readout->property("text").toString(), expectedReadout);

  // Every printed scale label sits where the model's gain law puts its gain.
  const qreal topY = fader->property("topY").toReal();
  const qreal travel = fader->property("travel").toReal();
  const struct {
    double db;
    const char *objectName;
  } labels[] = {
      {12.0, "consoleFaderScaleLabel_p12"}, {0.0, "consoleFaderScaleLabel_0"},
      {-12.0, "consoleFaderScaleLabel_m12"}, {-24.0, "consoleFaderScaleLabel_m24"},
      {-36.0, "consoleFaderScaleLabel_m36"}, {-48.0, "consoleFaderScaleLabel_m48"},
      {-60.0, "consoleFaderScaleLabel_m60"},
  };
  for (const auto &label : labels) {
    auto *item = findItem(page, QString::fromLatin1(label.objectName));
    QVERIFY2(item != nullptr, label.objectName);
    const qreal centre = item->property("y").toReal()
                         + item->property("height").toReal() / 2.0;
    const qreal expected = topY
        + (1.0 - QindaQt::Audio::faderPositionFromGainDb(label.db)) * travel;
    QVERIFY2(qAbs(centre - expected) < 0.51, label.objectName);
  }

  // Keyboard parity: the fader takes focus and steps, and Home returns to
  // unity — every step dispatched through the model as a POSITION.
  fader->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), fader);
  auto *faderAccessible = QAccessible::queryAccessibleInterface(fader);
  QVERIFY(faderAccessible != nullptr);
  QCOMPARE(faderAccessible->role(), QAccessible::Slider);

  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QTRY_VERIFY(m_model->lastFaderPosition > 0.0);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QVERIFY(qAbs(m_model->lastFaderPosition - (live + 0.02)) < 1e-9);

  QTest::keyClick(m_view.get(), Qt::Key_Home);
  QTRY_VERIFY(qAbs(m_model->lastFaderPosition
                   - QindaQt::Audio::unityFaderPosition()) < 1e-9);
}

// Strip-face controls dispatch their console intents: the pan dial, the
// mute lamp, and a routing-bank send lamp. These existed in the projection
// but pan had no surface at all before the rebuild.
void AudioConsolePageTest::consoleStripControlsDispatch() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  auto *pan = findItem(page, QStringLiteral("consoleStripPan_strip.hw.1"));
  QVERIFY(pan != nullptr);
  QVERIFY(pan->isEnabled());
  pan->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), pan);
  auto *panAccessible = QAccessible::queryAccessibleInterface(pan);
  QVERIFY(panAccessible != nullptr);
  QCOMPARE(panAccessible->role(), QAccessible::Slider);
  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  // Pan runs -1..+1 in 40 steps (ADR-0177 balance).
  QVERIFY(qAbs(m_model->lastFaderPosition - 0.05) < 1e-9);

  auto *mute = findItem(page, QStringLiteral("consoleStripMute_strip.hw.1"));
  QVERIFY(mute != nullptr);
  QVERIFY(mute->isEnabled());
  mute->setProperty("checked", true);
  QVERIFY(QMetaObject::invokeMethod(mute, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QCOMPARE(m_model->lastConsoleFlag, true);

  // A routing-bank lamp toggles its send and keeps the send's dialled gain.
  auto *send = findItem(page, QStringLiteral("consoleSend_strip.hw.1_0"));
  QVERIFY(send != nullptr);
  QVERIFY(send->isEnabled());
  send->setProperty("checked", false);
  QVERIFY(QMetaObject::invokeMethod(send, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QCOMPARE(m_model->lastBusIndex, 0);
  QCOMPARE(m_model->lastConsoleFlag, false);
}

// Bus-face controls dispatch their console intents: a channel-mode lamp, the
// mute lamp, and the bus fader's keyboard travel.
void AudioConsolePageTest::consoleBusControlsDispatch() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  auto *swap = findItem(page, QStringLiteral("consoleBusMode_bus.a1_swap"));
  QVERIFY(swap != nullptr);
  QVERIFY(swap->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(swap, "clicked"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.a1"));
  QCOMPARE(m_model->lastProcessing.value(QStringLiteral("mode")).toString(),
           QStringLiteral("swap"));

  auto *mute = findItem(page, QStringLiteral("consoleBusMute_bus.a1"));
  QVERIFY(mute != nullptr);
  mute->setProperty("checked", true);
  QVERIFY(QMetaObject::invokeMethod(mute, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.a1"));
  QCOMPARE(m_model->lastConsoleFlag, true);

  auto *fader = findItem(page, QStringLiteral("consoleBusFader_bus.b1"));
  QVERIFY(fader != nullptr);
  fader->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), fader);
  QTest::keyClick(m_view.get(), Qt::Key_End);
  QTRY_VERIFY(m_model->lastFaderPosition >= 0.0);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.b1"));
  QCOMPARE(m_model->lastFaderPosition, 0.0);
}


QTEST_MAIN(AudioConsolePageTest)
#include "tst_audio_console_page.moc"
