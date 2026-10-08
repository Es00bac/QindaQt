// SPDX-License-Identifier: GPL-3.0-or-later

// The compact Devices tab and its latency-offset control (ADR-0288), through
// the production AudioPage.qml against the duck-typed stub: the field names
// its device for assistive technology, dispatches whole milliseconds, keeps an
// unknown offset absent and a read-only one unclamped, sits in the keyboard
// traversal after mute and Details, and the compact rows stay dense.

#include "audio_page_test_support.h"

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/QQuickView>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsAudio::TestSupport::createAudioPage;
using QindaQt::Apps::SettingsAudio::TestSupport::findItem;
using QindaQt::Apps::SettingsAudio::TestSupport::prepareAudioPageEngine;

namespace {
bool openDetails(QQuickView &view, QQuickItem *page, const QString &name) {
  auto *details = findItem(page, name);
  if (details == nullptr || !details->isVisible() || !details->isEnabled())
    return false;
  details->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(&view, Qt::Key_Space);
  QCoreApplication::processEvents();
  return details->property("checked").toBool();
}
}

class AudioLatencyPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void fieldIsAccessibleBoundedAndDispatches();
  void unknownIsAbsentAndReadOnlyIsNotClamped();
  void latencyFollowsMuteInKeyboardTraversal();
  void compactRowsStayDense();

private:
  std::unique_ptr<QQuickView> m_view;
};

void AudioLatencyPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(prepareAudioPageEngine(*m_view, &error), qPrintable(error));
}

void AudioLatencyPageTest::fieldIsAccessibleBoundedAndDispatches() {
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(*m_view, model, QSize(900, 760));
  QVERIFY(page != nullptr);
  QVERIFY(openDetails(*m_view, page, QStringLiteral("audioOutputDetails_10")));
  auto *field = findItem(page, QStringLiteral("audioOutputLatency_10"));
  auto *reset = findItem(page, QStringLiteral("audioOutputLatencyReset_10"));
  QVERIFY(field != nullptr);
  QVERIFY(reset != nullptr);
  QVERIFY(field->isVisible());
  QVERIFY(field->isEnabled());
  QCOMPARE(field->property("value").toInt(), 40);
  QCOMPARE(field->property("from").toInt(), 0);
  QCOMPARE(field->property("to").toInt(), 2'000);

  auto *accessible = QAccessible::queryAccessibleInterface(field);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::SpinBox);
  QVERIFY(accessible->text(QAccessible::Name)
              .contains(QStringLiteral("Desk Speakers latency offset")));

  // A step is one whole-millisecond intent; the host owns the value.
  QVERIFY(QMetaObject::invokeMethod(field, "step", Q_ARG(QVariant, 1),
                                    Q_ARG(QVariant, 0)));
  QCOMPARE(model.latencySerial, qulonglong(10));
  QCOMPARE(model.latencyMs, 45);
  QCOMPARE(field->property("value").toInt(), 40);
  // A typed value outside the device's range is clamped before dispatch.
  QVERIFY(QMetaObject::invokeMethod(field, "commit", Q_ARG(QVariant, -30)));
  QCOMPARE(model.latencyMs, 0);

  QVERIFY(reset->property("available").toBool());
  QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
  QCOMPARE(model.latencyCount, 3);
  QCOMPARE(model.latencyMs, 0);
  auto *resetAccessible = QAccessible::queryAccessibleInterface(reset);
  QVERIFY(resetAccessible != nullptr);
  QVERIFY(resetAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("latency offset")));

  // The signed (Bluetooth-like) device admits negative offsets; its reset is
  // unavailable because it already reads 0 ms.
  QVERIFY(openDetails(*m_view, page, QStringLiteral("audioOutputDetails_12")));
  auto *signedField = findItem(page, QStringLiteral("audioOutputLatency_12"));
  QVERIFY(signedField != nullptr);
  QCOMPARE(signedField->property("from").toInt(), -2'000);
  QVERIFY(!findItem(page, QStringLiteral("audioOutputLatencyReset_12"))
               ->property("available").toBool());
}

void AudioLatencyPageTest::unknownIsAbsentAndReadOnlyIsNotClamped() {
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(*m_view, model, QSize(900, 760));
  QVERIFY(page != nullptr);
  QVERIFY(openDetails(*m_view, page, QStringLiteral("audioInputDetails_20")));
  // Known but not settable: shown, disabled, and never clamped into a
  // number the device did not report.
  auto *readOnly = findItem(page, QStringLiteral("audioInputLatency_20"));
  QVERIFY(readOnly != nullptr);
  QVERIFY(readOnly->isVisible());
  QVERIFY(!readOnly->isEnabled());
  QCOMPARE(readOnly->property("clampedValue").toInt(), 15);
  QVERIFY(!findItem(page, QStringLiteral("audioInputLatencyReset_20"))->isEnabled());
  // Unknown: no field and no reset at all.
  auto *unknown = findItem(page, QStringLiteral("audioInputLatency_22"));
  QVERIFY(unknown == nullptr || !unknown->isVisible());
  auto *unknownReset = findItem(page, QStringLiteral("audioInputLatencyReset_22"));
  QVERIFY(unknownReset == nullptr || !unknownReset->isVisible());
}

void AudioLatencyPageTest::latencyFollowsMuteInKeyboardTraversal() {
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(*m_view, model, QSize(420, 320));
  QVERIFY(page != nullptr);
  QVERIFY(openDetails(*m_view, page, QStringLiteral("audioOutputDetails_10")));
  auto *mute = findItem(page, QStringLiteral("audioOutputMute_10"));
  auto *details = findItem(page, QStringLiteral("audioOutputDetails_10"));
  auto *field = findItem(page, QStringLiteral("audioOutputLatency_10"));
  auto *reset = findItem(page, QStringLiteral("audioOutputLatencyReset_10"));
  QVERIFY(mute != nullptr);
  QVERIFY(details != nullptr);
  QVERIFY(field != nullptr);
  QVERIFY(reset != nullptr);
  auto *input = field->property("inputItem").value<QQuickItem *>();
  QVERIFY(input != nullptr);

  mute->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), mute);
  QTest::keyClick(m_view.get(), Qt::Key_Tab);
  QTRY_COMPARE(m_view->activeFocusItem(), details);
  QTest::keyClick(m_view.get(), Qt::Key_Tab);
  QTRY_COMPARE(m_view->activeFocusItem(), input);
  // Arrow keys step the focused field, Shift for ten steps.
  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QCOMPARE(model.latencyMs, 45);
  QTest::keyClick(m_view.get(), Qt::Key_Up, Qt::ShiftModifier);
  QCOMPARE(model.latencyMs, 90);
  QTest::keyClick(m_view.get(), Qt::Key_Tab);
  QTRY_COMPARE(m_view->activeFocusItem(), reset);
  const auto writes = model.latencyCount;
  details->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTRY_VERIFY(!field->isVisible());
  QTRY_COMPARE(m_view->activeFocusItem(), details);
  QCOMPARE(model.latencyCount, writes);
}

void AudioLatencyPageTest::compactRowsStayDense() {
  StubAudioSettingsModel model;
  auto [guard, page] = createAudioPage(*m_view, model, QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *first = findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *second = findItem(page, QStringLiteral("audioOutputVolume_12"));
  QVERIFY(first != nullptr);
  QVERIFY(second != nullptr);
  // Common device controls occupy two lines. Advanced latency/channel
  // controls remain available through Details without reserving row space.
  const qreal pitch = second->mapToScene(QPointF()).y()
                      - first->mapToScene(QPointF()).y();
  QVERIFY2(pitch > 0.0 && pitch <= 64.0, qPrintable(QString::number(pitch)));
  // Both outputs are on screen at the compact size without scrolling.
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(viewport != nullptr);
  QCOMPARE(viewport->property("contentY").toReal(), 0.0);
  QVERIFY(second->mapToItem(viewport, QPointF(0, second->height())).y()
          <= viewport->height());
}

QTEST_MAIN(AudioLatencyPageTest)
#include "tst_audio_latency_page.moc"
