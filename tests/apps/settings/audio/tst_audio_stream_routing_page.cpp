// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_page_test_support.h"

#include <QtGui/QAccessible>
#include <QtGui/QWheelEvent>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Apps::SettingsAudio::TestSupport;

namespace {
void reveal(QQuickItem *page, QQuickItem *item) {
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  Q_ASSERT(viewport != nullptr);
  const qreal current = viewport->property("contentY").toReal();
  const qreal viewportCenter =
      viewport->mapToScene(QPointF(0, viewport->height() / 2)).y();
  const qreal itemCenter =
      item->mapToScene(QPointF(0, item->height() / 2)).y();
  const qreal maximum = qMax(0.0,
      viewport->property("contentHeight").toReal() - viewport->height());
  viewport->setProperty("contentY",
      qBound(0.0, current + itemCenter - viewportCenter, maximum));
  QCoreApplication::processEvents();
}

void wheel(QQuickItem *item, const QPoint delta) {
  auto *window = item->window();
  Q_ASSERT(window != nullptr);
  const QPointF scene = item->mapToScene(
      QPointF(item->width() / 2, item->height() / 2));
  QWheelEvent event(scene, window->mapToGlobal(scene.toPoint()), {}, delta,
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(window, &event);
  QCoreApplication::processEvents();
}
} // namespace

class AudioStreamRoutingPageTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void compactPickerKeyboardAndWheel();
  void emptyStreamAndVirtualListsShowEmptyStates();
};

void AudioStreamRoutingPageTest::compactPickerKeyboardAndWheel() {
  QQuickView view;
  QString error;
  QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(view, model, QSize(420, 320));
  QVERIFY(page != nullptr);
  QCOMPARE(page->property("activeTab").toInt(), 0);
  auto *playback = findItem(page, QStringLiteral("audioStreamTarget_30"));
  auto *recording = findItem(page, QStringLiteral("audioStreamTarget_40"));
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(playback != nullptr);
  QVERIFY(recording != nullptr);
  QVERIFY(viewport != nullptr);
  QVERIFY(playback->isEnabled());
  QVERIFY(recording->isEnabled());
  QCOMPARE(playback->property("currentIndex").toInt(), 0);
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Desk Speakers"));
  QCOMPARE(recording->property("currentIndex").toInt(), 0);
  QVERIFY(!playback->property("wheelEnabled").toBool());
  auto *accessible = QAccessible::queryAccessibleInterface(playback);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::ComboBox);
  QVERIFY(accessible->text(QAccessible::Name).contains(
      QStringLiteral("Player")));

  reveal(page, playback);
  const qreal beforeWheel = viewport->property("contentY").toReal();
  QVERIFY(beforeWheel > 0);
  wheel(playback, QPoint(0, 120));
  QTRY_VERIFY(viewport->property("contentY").toReal() < beforeWheel);
  QCOMPARE(model.movedStreamSerial, qulonglong(0));
  QCOMPARE(playback->property("currentIndex").toInt(), 0);
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Desk Speakers"));

  // The same compact row remains reachable by keyboard after the wheel
  // scrolls the page; only deliberate selection dispatches the move.
  reveal(page, playback);
  playback->forceActiveFocus(Qt::TabFocusReason);
  QVERIFY(playback->hasActiveFocus());
  QTest::keyClick(&view, Qt::Key_Down);
  QTRY_COMPARE(model.movedStreamSerial, qulonglong(30));
  QCOMPARE(model.movedDeviceSerial, qulonglong(12));
  // The stub does not echo an authoritative snapshot. The visible target
  // must remain on the old device rather than assuming the move succeeded.
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Desk Speakers"));
  QTRY_COMPARE(playback->property("currentIndex").toInt(), 0);

  // A refused keyboard move is still only intent: the ComboBox must not
  // retain its internally changed index after the model returns false.
  model.acceptStreamMove = false;
  const int beforeRefusal = model.moveStreamCalls;
  QTest::keyClick(&view, Qt::Key_Down);
  QTRY_COMPARE(model.moveStreamCalls, beforeRefusal + 1);
  QTRY_COMPARE(playback->property("currentIndex").toInt(), 0);
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Desk Speakers"));
  model.acceptStreamMove = true;
  auto refusedRow = model.streams.first().toMap();
  refusedRow.insert(QStringLiteral("routeErrorText"),
                    QStringLiteral("Selected device did not take effect"));
  model.streams[0] = refusedRow;
  Q_EMIT model.viewChanged();
  auto *routeError = findItem(page, QStringLiteral("audioStreamTargetError_30"));
  QVERIFY(routeError != nullptr);
  QTRY_VERIFY(routeError->isVisible());
  QCOMPARE(routeError->property("text").toString(),
           QStringLiteral("Selected device did not take effect"));
  refusedRow.remove(QStringLiteral("routeErrorText"));
  model.streams[0] = refusedRow;
  Q_EMIT model.viewChanged();
  QTRY_VERIFY(!routeError->isVisible());

  // An accepted snapshot may reorder the device list. Selection is derived
  // from the target serial, never the previously clicked list index.
  auto confirmed = model.streams.first().toMap();
  confirmed.insert(QStringLiteral("targetSerial"), qulonglong(12));
  confirmed.insert(QStringLiteral("targetName"),
                   QStringLiteral("Surround Headphones"));
  confirmed.insert(QStringLiteral("targetChoices"),
      QVariantList{QVariantMap{{QStringLiteral("serial"), qulonglong(12)},
                               {QStringLiteral("label"), QStringLiteral("Surround Headphones")}},
                   QVariantMap{{QStringLiteral("serial"), qulonglong(10)},
                               {QStringLiteral("label"), QStringLiteral("Desk Speakers")}}});
  model.streams[0] = confirmed;
  Q_EMIT model.viewChanged();
  QTRY_COMPARE(playback->property("currentIndex").toInt(), 0);
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Surround Headphones"));
  confirmed.insert(QStringLiteral("targetChoices"),
      QVariantList{QVariantMap{{QStringLiteral("serial"), qulonglong(10)},
                               {QStringLiteral("label"), QStringLiteral("Desk Speakers")}},
                   QVariantMap{{QStringLiteral("serial"), qulonglong(12)},
                               {QStringLiteral("label"), QStringLiteral("Surround Headphones")}}});
  model.streams[0] = confirmed;
  Q_EMIT model.viewChanged();
  QTRY_COMPARE(playback->property("currentIndex").toInt(), 1);
  confirmed.insert(QStringLiteral("targetSerial"), qulonglong(0));
  confirmed.insert(QStringLiteral("targetName"), QStringLiteral("Unknown device"));
  confirmed.insert(QStringLiteral("targetChoices"),
      QVariantList{QVariantMap{{QStringLiteral("serial"), qulonglong(10)},
                               {QStringLiteral("label"), QStringLiteral("Desk Speakers")}}});
  confirmed.insert(QStringLiteral("moveAvailable"), false);
  model.streams[0] = confirmed;
  Q_EMIT model.viewChanged();
  QTRY_COMPARE(playback->property("currentIndex").toInt(), -1);
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Unknown device"));

  QVERIFY(QMetaObject::invokeMethod(recording, "activated", Q_ARG(int, 1)));
  QCOMPARE(model.movedStreamSerial, qulonglong(40));
  QCOMPARE(model.movedDeviceSerial, qulonglong(22));
  auto row = model.streams.first().toMap();
  row.insert(QStringLiteral("moveAvailable"), false);
  model.streams[0] = row;
  Q_EMIT model.viewChanged();
  QTRY_VERIFY(!playback->isEnabled());
  QCOMPARE(playback->property("displayText").toString(),
           QStringLiteral("Unknown device"));
}

void AudioStreamRoutingPageTest::emptyStreamAndVirtualListsShowEmptyStates() {
  QQuickView view;
  QString error;
  QVERIFY2(prepareAudioPageEngine(view, &error), qPrintable(error));
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(view, model, QSize(900, 760));
  QVERIFY(page != nullptr);
  auto *streams = findItem(page, QStringLiteral("audioStreamsEmpty"));
  auto *virtuals = findItem(page, QStringLiteral("audioVirtualEmpty"));
  QVERIFY(streams != nullptr);
  QVERIFY(virtuals != nullptr);
  // Populated lists show rows, not the empty states.
  QVERIFY(!streams->isVisible());
  QVERIFY(!virtuals->isVisible());

  model.streams.clear();
  model.virtualDevices.clear();
  Q_EMIT model.viewChanged();
  QTRY_VERIFY(streams->isVisible() && streams->height() > 0);
  QTRY_VERIFY(virtuals->isVisible() && virtuals->height() > 0);
  // The add actions stay reachable while the virtual list is empty.
  auto *addOutput = findItem(page, QStringLiteral("audioVirtualAddOutput"));
  QVERIFY(addOutput != nullptr);
  QVERIFY(addOutput->isVisible());
  for (auto *empty : {streams, virtuals}) {
    auto *accessible = QAccessible::queryAccessibleInterface(empty);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::StaticText);
    QCOMPARE(accessible->text(QAccessible::Name), empty->property("text").toString());
  }
  QCOMPARE(streams->property("text").toString(),
           QStringLiteral("No application streams are currently reported."));
}

QTEST_MAIN(AudioStreamRoutingPageTest)
#include "tst_audio_stream_routing_page.moc"
