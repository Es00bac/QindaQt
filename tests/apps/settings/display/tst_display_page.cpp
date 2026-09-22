// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"
#include "display_page_test_support.h"
#include "stub_display_model.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include <memory>
#include <utility>

namespace {

namespace PageSupport = QindaQt::Tests::DisplayPageSupport;
using QindaQt::Tests::DisplayPageSupport::findItemByObjectName;
using QindaQt::Tests::DisplayPageSupport::StubDisplayModel;

const char *const BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
const char *const DisplayPageQmlPath = QINDAQT_DISPLAY_PAGE_QML_PATH;

} // namespace

class DisplayPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testPageRenderingAndControls();
  void testScaleAndOrientationInteraction();
  void testMirrorRowOffersExtendAndEveryOtherOutput();
  void testOutputCardNamesRotationAndMirroring();
  void testArrangementPositionSynchronizationOnSwitchAndRevert();
  void testAbandonedPositionEditCannotCrossOutputSelection();
  void testExternalPositionRefreshCannotBeResurrectedOnBlur();
  void testOutputCardsSupportKeyboardRadioSelection();
  void testDisabledConnectedOutputCanBeEnabled();
  void testUnavailableNoticeAndRetry();
  void testPreviewBannerAndTransactionActions();
  void testNullModelBindsWithoutTypeErrors();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubDisplayModel> m_model;
};

void DisplayPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QString::fromUtf8(BuildQmlImportPath));

  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));

  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString pubError;
  QVERIFY2(facade->publish(loaded.theme, {}, &pubError), qPrintable(pubError));

  m_model = std::make_unique<StubDisplayModel>();
}

namespace {

// QML binding errors surface as qWarning() lines, which is exactly how they
// reached the session log. Capture them so a test can assert their absence.
QStringList g_capturedWarnings;
QtMessageHandler g_previousHandler = nullptr;

void captureWarnings(QtMsgType type, const QMessageLogContext &context,
                     const QString &message) {
  if (type == QtWarningMsg) {
    g_capturedWarnings.append(message);
  }
  if (g_previousHandler != nullptr) {
    g_previousHandler(type, context, message);
  }
}

} // namespace

// The Display route is handed `displaySettings: null` whenever the page is
// constructed before its model is assigned, and again while the route is torn
// down. Bindings that dereferenced through it threw, and a thrown binding does
// not merely log -- the whole binding fails, so `candidates` evaluated empty
// and the mirror control disabled itself. One session logged 110 of these from
// SettingsRouteHost alone.
void DisplayPageTest::testNullModelBindsWithoutTypeErrors() {
  g_capturedWarnings.clear();
  g_previousHandler = qInstallMessageHandler(captureWarnings);

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"), QVariant::fromValue<QObject *>(nullptr)},
  });
  std::unique_ptr<QObject> pageGuard(pageObj);
  if (pageObj != nullptr) {
    if (auto *pageItem = qobject_cast<QQuickItem *>(pageObj)) {
      PageSupport::attachPage(*m_view, *pageItem);
      QTest::qWait(50);
    }
  }

  qInstallMessageHandler(g_previousHandler);
  g_previousHandler = nullptr;

  QStringList typeErrors;
  for (const QString &warning : std::as_const(g_capturedWarnings)) {
    // Not just TypeError: replacing a throw with an undefined assignment
    // would trade one warning for another and still leave the binding wrong.
    if (warning.contains(QStringLiteral("TypeError"))
        || warning.contains(QStringLiteral("Unable to assign"))) {
      typeErrors.append(warning);
    }
  }
  QVERIFY2(typeErrors.isEmpty(),
           qPrintable(QStringLiteral("null displaySettings produced %1 binding "
                                     "error(s):\n%2")
                          .arg(typeErrors.size())
                          .arg(typeErrors.join(QLatin1Char('\n')))));
  QVERIFY(pageObj != nullptr);
}

void DisplayPageTest::testPageRenderingAndControls() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  auto *heading = findItemByObjectName(pageItem, QStringLiteral("displayPageHeading"));
  QVERIFY(heading != nullptr);

  auto *applyBtn = findItemByObjectName(pageItem, QStringLiteral("displayApplyButton"));
  QVERIFY(applyBtn != nullptr);
  QCOMPARE(applyBtn->property("available").toBool(), false);

  QVERIFY(findItemByObjectName(pageItem, QStringLiteral("displayCloseButton")) == nullptr);
}

void DisplayPageTest::testScaleAndOrientationInteraction() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  // Find 100% scale button and click it
  auto *scaleBtn100 = findItemByObjectName(pageItem, QStringLiteral("displayScaleButton_100"));
  QVERIFY(scaleBtn100 != nullptr);
  QMetaObject::invokeMethod(scaleBtn100, "clicked");

  QCOMPARE(m_model->selectedOutput.value(QStringLiteral("scale")).toDouble(), 1.0);
  QVERIFY(m_model->draftDirty);

  // Check apply button is now available
  auto *applyBtn = findItemByObjectName(pageItem, QStringLiteral("displayApplyButton"));
  QVERIFY(applyBtn != nullptr);
  QCOMPARE(applyBtn->property("available").toBool(), true);

  QMetaObject::invokeMethod(applyBtn, "clicked");
  QCOMPARE(m_model->appliedCount, 1);
}

// ADR-0190: the mirror row is the only surface that writes
// `replicationSourceStableId`, so its choice set is the contract. It offers
// Extend plus one button per *other enabled, not-itself-mirrored* output,
// which is exactly what the model accepts — offering a choice the model
// refuses would be a dead control.
void DisplayPageTest::testMirrorRowOffersExtendAndEveryOtherOutput() {
  // Two outputs: with one there is nothing to mirror onto, which the row says
  // rather than offering a dead button.
  m_model->setupTwoOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;
  m_model->mirrorSetCount = 0;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *row = findItemByObjectName(pageItem, QStringLiteral("displayMirrorChoiceRow"));
  QVERIFY2(row != nullptr, "the mirror row is not on the page");
  auto *extendBtn =
      findItemByObjectName(pageItem, QStringLiteral("displayMirrorExtendButton"));
  QVERIFY(extendBtn != nullptr);
  // Extend is the current state: nothing is mirroring anything yet.
  QCOMPARE(extendBtn->property("checked").toBool(), true);

  // The second stub output is HDMI-1; the selected one is DP-1.
  auto *mirrorBtn =
      findItemByObjectName(pageItem, QStringLiteral("displayMirrorButton_HDMI-1"));
  QVERIFY2(mirrorBtn != nullptr, "no mirror button for the other output");
  QCOMPARE(mirrorBtn->property("checked").toBool(), false);
  QCOMPARE(mirrorBtn->property("available").toBool(), true);
  // An output is never offered as its own mirror source.
  QVERIFY(findItemByObjectName(pageItem, QStringLiteral("displayMirrorButton_DP-1"))
          == nullptr);

  QMetaObject::invokeMethod(mirrorBtn, "clicked");
  QCOMPARE(m_model->mirrorSetCount, 1);
  QCOMPARE(m_model->lastMirrorStableId, m_model->selectedOutputId);
  QVERIFY(!m_model->lastMirrorSourceId.isEmpty());
  QCOMPARE(m_model->selectedOutput
               .value(QStringLiteral("replicationSourceStableId"))
               .toString(),
           m_model->lastMirrorSourceId);

  // Extend clears it, through the same operation with an empty source.
  QMetaObject::invokeMethod(extendBtn, "clicked");
  QCOMPARE(m_model->mirrorSetCount, 2);
  QVERIFY(m_model->lastMirrorSourceId.isEmpty());
}

// The reported defect was "there is no rotation control" on a laptop whose
// Display1 reported the output enabled and editable: the Orientation section
// is the fifth on a long page, and nothing above it said the display was
// rotated. The card now says so.
void DisplayPageTest::testOutputCardNamesRotationAndMirroring() {
  m_model->setupTwoOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *summary =
      findItemByObjectName(pageItem, QStringLiteral("displayOutputCardStateSummary"));
  QVERIFY(summary != nullptr);
  // A normal, unmirrored display says nothing extra rather than padding the
  // card with "Rotated 0°".
  QCOMPARE(summary->property("text").toString(), QString());
  QCOMPARE(summary->property("visible").toBool(), false);

  auto *orientBtn =
      findItemByObjectName(pageItem, QStringLiteral("displayOrientationButton_90"));
  QVERIFY(orientBtn != nullptr);
  QMetaObject::invokeMethod(orientBtn, "clicked");

  summary = findItemByObjectName(pageItem,
                                 QStringLiteral("displayOutputCardStateSummary"));
  QVERIFY(summary != nullptr);
  QCOMPARE(summary->property("visible").toBool(), true);
  QVERIFY2(summary->property("text").toString().contains(QStringLiteral("90")),
           qPrintable(summary->property("text").toString()));

  auto *mirrorBtn =
      findItemByObjectName(pageItem, QStringLiteral("displayMirrorButton_HDMI-1"));
  QVERIFY(mirrorBtn != nullptr);
  QMetaObject::invokeMethod(mirrorBtn, "clicked");
  summary = findItemByObjectName(pageItem,
                                 QStringLiteral("displayOutputCardStateSummary"));
  QVERIFY(summary != nullptr);
  const QString both = summary->property("text").toString();
  QVERIFY2(both.contains(QStringLiteral("90")), qPrintable(both));
  QVERIFY2(both.contains(QStringLiteral("Mirror")), qPrintable(both));
}

void DisplayPageTest::testArrangementPositionSynchronizationOnSwitchAndRevert() {
  m_model->setupTwoOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  auto *posXField = findItemByObjectName(pageItem, QStringLiteral("displayPosXField"));
  auto *posYField = findItemByObjectName(pageItem, QStringLiteral("displayPosYField"));
  QVERIFY(posXField != nullptr);
  QVERIFY(posYField != nullptr);

  // 1. Initial output DP-1 position is (0, 0)
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("0"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));

  // 2. A valid position crosses into the model at Return/Enter.
  m_model->positionSetCount = 0;
  posXField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posXField);
  PageSupport::replaceFocusedText(*m_view, QStringLiteral("500"));
  QCOMPARE(m_model->positionSetCount, 0);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_model->positionSetCount, 1);
  QCOMPARE(m_model->lastPositionStableId, QStringLiteral("edid:dp1"));
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("500"));

  // 3. Switch selected output to HDMI-1 (position (1920, 0))
  // The focus-safe synchronization must immediately refresh posXField text to "1920"
  m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("1920"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));

  // 4. Switch back to DP-1 (which has drafted position 500)
  m_model->setSelectedOutputId(QStringLiteral("edid:dp1"));
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("500"));

  // 5. A valid edit also commits when the user moves to the next field.
  posXField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posXField);
  PageSupport::replaceFocusedText(*m_view, QStringLiteral("640"));
  posYField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posYField);
  QTRY_COMPARE(m_model->positionSetCount, 2);
  QCOMPARE(m_model->outputsMap.value(QStringLiteral("edid:dp1"))
               .value(QStringLiteral("positionX")).toInt(), 640);

  // 6. Cancel / revert draft - restores baseline position (0, 0)
  m_model->cancelDraft();
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("0"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));

  // 7. Malformed text still cannot cross the model boundary.
  posXField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posXField);
  PageSupport::replaceFocusedText(*m_view, QStringLiteral("-"));
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QCoreApplication::processEvents();
  QCOMPARE(m_model->positionSetCount, 2);
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("0"));
}

void DisplayPageTest::testAbandonedPositionEditCannotCrossOutputSelection() {
  m_model->setupTwoOutputs();
  m_model->positionSetCount = 0;
  m_model->lastPositionStableId.clear();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  auto *posXField = findItemByObjectName(pageItem, QStringLiteral("displayPosXField"));
  QVERIFY(posXField != nullptr);
  posXField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posXField);
  PageSupport::replaceFocusedText(*m_view, QStringLiteral("777"));
  QCOMPARE(m_model->positionSetCount, 0);

  m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));
  QCoreApplication::processEvents();

  QCOMPARE(m_model->positionSetCount, 0);
  QCOMPARE(m_model->outputsMap.value(QStringLiteral("edid:hdmi1"))
               .value(QStringLiteral("positionX")).toInt(), 1920);
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("1920"));
}

void DisplayPageTest::testExternalPositionRefreshCannotBeResurrectedOnBlur() {
  m_model->setupTwoOutputs();
  m_model->positionSetCount = 0;
  m_model->lastPositionStableId.clear();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  auto *posXField = findItemByObjectName(pageItem, QStringLiteral("displayPosXField"));
  auto *posYField = findItemByObjectName(pageItem, QStringLiteral("displayPosYField"));
  QVERIFY(posXField != nullptr);
  QVERIFY(posYField != nullptr);
  posXField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posXField);
  PageSupport::replaceFocusedText(*m_view, QStringLiteral("777"));
  QCOMPARE(m_model->positionSetCount, 0);

  m_model->publishExternalOutputPosition(QStringLiteral("edid:dp1"), 640, 40);
  QCoreApplication::processEvents();
  QCOMPARE(m_model->positionSetCount, 0);
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("640"));
  posYField->forceActiveFocus(Qt::OtherFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), posYField);
  QCoreApplication::processEvents();

  QCOMPARE(m_model->positionSetCount, 0);
  QCOMPARE(m_model->outputsMap.value(QStringLiteral("edid:dp1"))
               .value(QStringLiteral("positionX")).toInt(), 640);
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("640"));
}

void DisplayPageTest::testOutputCardsSupportKeyboardRadioSelection() {
  m_model->setupTwoOutputs();
  m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  const auto cards = PageSupport::outputCards(pageItem);
  QCOMPARE(cards.size(), 2);
  QQuickItem *dpCard = nullptr;
  for (auto *card : cards) {
    const auto data = card->property("outputData").toMap();
    if (data.value(QStringLiteral("stableId")).toString()
        == QStringLiteral("edid:dp1")) {
      dpCard = card;
      break;
    }
  }
  QVERIFY(dpCard != nullptr);
  QVERIFY(dpCard->activeFocusOnTab());
  dpCard->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), dpCard);
  const QList<Qt::Key> activationKeys{Qt::Key_Return, Qt::Key_Enter,
                                      Qt::Key_Space};
  for (const auto key : activationKeys) {
    m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));
    dpCard->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(m_view->activeFocusItem(), dpCard);
    QTest::keyClick(m_view.get(), key);
    QCoreApplication::processEvents();
    QCOMPARE(m_model->selectedOutputId, QStringLiteral("edid:dp1"));
  }
  auto *accessible = QAccessible::queryAccessibleInterface(dpCard);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::RadioButton);
  QVERIFY(accessible->state().checkable);
  QVERIFY(accessible->state().checked);
}

void DisplayPageTest::testDisabledConnectedOutputCanBeEnabled() {
  m_model->setupTwoOutputs();
  auto projector = m_model->outputsMap.value(QStringLiteral("edid:hdmi1"));
  projector[QStringLiteral("enabled")] = false;
  m_model->outputsMap[QStringLiteral("edid:hdmi1")] = projector;
  m_model->baselineOutputsMap = m_model->outputsMap;
  m_model->outputs = {m_model->outputsMap.value(QStringLiteral("edid:dp1")), projector};
  m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);
  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);
  PageSupport::attachPage(*m_view, *pageItem);

  auto *enableSwitch =
      findItemByObjectName(pageItem, QStringLiteral("displayEnableSwitch"));
  QVERIFY(enableSwitch != nullptr);
  QCOMPARE(enableSwitch->property("checked").toBool(), false);

  const QPoint switchCenter = enableSwitch->mapToScene(
      QPointF(enableSwitch->width() / 2.0, enableSwitch->height() / 2.0)).toPoint();
  QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier, switchCenter);
  QTRY_COMPARE(m_model->selectedOutput.value(QStringLiteral("enabled")).toBool(),
               true);
  QVERIFY(m_model->draftDirty);
  QVERIFY(m_model->applyAvailable);
}

void DisplayPageTest::testUnavailableNoticeAndRetry() {
  m_model->unavailable = true;
  m_model->statusText = QStringLiteral("Service unavailable");
  m_model->outputs.clear();
  m_model->selectedOutput.clear();
  Q_EMIT m_model->stateChanged();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *notice = findItemByObjectName(pageItem, QStringLiteral("displayUnavailableNotice"));
  QVERIFY(notice != nullptr);
  QVERIFY(notice->isVisible());

  QVERIFY(QMetaObject::invokeMethod(notice, "retryRequested"));
  QCOMPARE(m_model->retriedCount, 1);
}

void DisplayPageTest::testPreviewBannerAndTransactionActions() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = true;
  m_model->awaitingConfirmation = true;
  m_model->transactionStatusText = QStringLiteral("Preview active. Reverting in 15s");
  Q_EMIT m_model->transactionChanged();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *previewBanner = findItemByObjectName(pageItem, QStringLiteral("displayPreviewBanner"));
  QVERIFY(previewBanner != nullptr);
  QVERIFY(previewBanner->isVisible());

  auto *keepBtn = findItemByObjectName(pageItem, QStringLiteral("displayPreviewKeepButton"));
  QVERIFY(keepBtn != nullptr);
  QMetaObject::invokeMethod(keepBtn, "clicked");
  QCOMPARE(m_model->confirmedCount, 1);

  auto *revertBtn = findItemByObjectName(pageItem, QStringLiteral("displayPreviewRevertButton"));
  QVERIFY(revertBtn != nullptr);
  QMetaObject::invokeMethod(revertBtn, "clicked");
  QCOMPARE(m_model->revertedCount, 1);
}

QTEST_MAIN(DisplayPageTest)
#include "tst_display_page.moc"
