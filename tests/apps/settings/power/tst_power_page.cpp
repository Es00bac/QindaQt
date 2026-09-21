// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_power_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <algorithm>
#include <memory>

using QindaQt::Apps::SettingsPower::TestSupport::StubExternalBrightness;
using QindaQt::Apps::SettingsPower::TestSupport::StubPowerSettingsModel;
using QindaQt::Apps::SettingsPower::TestSupport::StubScreenLockSettings;
using QindaQt::Apps::SettingsPower::TestSupport::StubIdleDisplaySettings;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}
} // namespace

class PowerPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersWideTruthAndAccessibleControls();
  void routesKeyboardAndProfileActions();
  void internalSliderStaysResponsiveThroughRepublication();
  void externalDisplayRowsOfferAdmittedSlidersAndReadOnlyReasons();
  void sessionActionsHaveKeyboardParityAndDestructiveConfirmation();
  void compactAndUnavailableFocusRemainAdmitted();
  void screenLockControlsRespectAutomaticLock();
  void idleDisplayControlsRespectThePolicy();
  void resumeLockAndGraceRowsBindAndApply();
  void powerPolicyRowsRespectLidPresenceAndApply();
  void focusChainReachesTheLidPolicySection();
  void automaticProfileRowsBuildOptionsAndApplyPerSource();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubPowerSettingsModel> m_model;
  StubScreenLockSettings *m_screenLock = nullptr;
  StubIdleDisplaySettings *m_idleDisplay = nullptr;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
  void scrollIntoView(QQuickItem *page, QQuickItem *item);
};

void PowerPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &error);
  QVERIFY2(facade != nullptr, qPrintable(error));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
PowerPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubPowerSettingsModel>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_POWER_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto *screenLock = new StubScreenLockSettings(m_model.get());
  m_screenLock = screenLock;
  auto *idleDisplay = new StubIdleDisplaySettings(m_model.get());
  m_idleDisplay = idleDisplay;
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("powerSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
      {QStringLiteral("screenLockSettings"),
       QVariant::fromValue(static_cast<QObject *>(screenLock))},
      {QStringLiteral("idleDisplaySettings"),
       QVariant::fromValue(static_cast<QObject *>(idleDisplay))},
  });
  if (object == nullptr) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto guard = std::unique_ptr<QObject>(object);
  auto *page = qobject_cast<QQuickItem *>(object);
  if (page == nullptr) return {};
  m_view->resize(size);
  page->setParentItem(m_view->contentItem());
  page->setSize(size);
  m_view->show();
  QCoreApplication::processEvents();
  return {std::move(guard), page};
}

// The Power form scrolls, and sections below the fold move whenever one is
// added above them. A synthetic click only reaches a row that is inside the
// viewport, so every mouse-driven assertion scrolls its target into view
// first rather than depending on the current section order.
void PowerPageTest::scrollIntoView(QQuickItem *page, QQuickItem *item) {
  QVERIFY(page != nullptr);
  QVERIFY(item != nullptr);
  auto *viewport = findItem(page, QStringLiteral("powerFormViewport"));
  QVERIFY(viewport != nullptr);
  const qreal contentHeight = viewport->property("contentHeight").toReal();
  const qreal maximum = std::max(qreal(0), contentHeight - viewport->height());
  const qreal contentY = viewport->property("contentY").toReal();
  const qreal offset = item->mapToItem(viewport, QPointF(0, 0)).y();
  const qreal centred =
      contentY + offset - (viewport->height() - item->height()) / 2;
  viewport->setProperty("contentY", std::clamp(centred, qreal(0), maximum));
  QCoreApplication::processEvents();
}

void PowerPageTest::rendersWideTruthAndAccessibleControls() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *lock = findItem(page, QStringLiteral("powerSessionLock"));
  auto *profile = findItem(page, QStringLiteral("powerProfile_balanced"));
  auto *internal = findItem(page,
      QStringLiteral("powerInternalBrightness_internal-41-1"));
  auto *keyboard = findItem(page,
      QStringLiteral("powerKeyboardBrightness_keyboard-41-1"));
  auto *raw = findItem(page, QStringLiteral("powerInternalRaw_internal-41-1"));
  QVERIFY(lock != nullptr);
  QVERIFY(profile != nullptr);
  QVERIFY(internal != nullptr);
  QVERIFY(keyboard != nullptr);
  QVERIFY(raw != nullptr);
  QVERIFY(internal->property("text").toString().contains(QStringLiteral("%")));
  QVERIFY(keyboard->isEnabled());
  QVERIFY(!raw->property("text").toString().contains(QStringLiteral("10000")));
  auto *sliderAccessible = QAccessible::queryAccessibleInterface(keyboard);
  QVERIFY(sliderAccessible != nullptr);
  QCOMPARE(sliderAccessible->role(), QAccessible::Slider);
  QVERIFY(sliderAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("%")));
  auto *lockAccessible = QAccessible::queryAccessibleInterface(lock);
  QVERIFY(lockAccessible != nullptr);
  QCOMPARE(lockAccessible->role(), QAccessible::Button);
  QVERIFY(lockAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("Lock")));
}

void PowerPageTest::sessionActionsHaveKeyboardParityAndDestructiveConfirmation() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *lock = findItem(page, QStringLiteral("powerSessionLock"));
  auto *suspend = findItem(page, QStringLiteral("powerSessionSuspend"));
  auto *restart = findItem(page, QStringLiteral("powerSessionRestart"));
  QVERIFY(lock != nullptr);
  QVERIFY(suspend != nullptr);
  QVERIFY(restart != nullptr);

  lock->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), lock);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(m_model->sessionActionState.requests,
           QStringList({QStringLiteral("lock")}));

  suspend->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(m_model->sessionActionState.requests.constLast(),
           QStringLiteral("suspend"));

  restart->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(m_model->sessionActionState.requests.size(), 2);
  QObject *confirmation = page->findChild<QObject *>(
      QStringLiteral("powerSessionConfirmation"));
  QVERIFY(confirmation != nullptr);
  QTRY_VERIFY(confirmation->property("opened").toBool());
  QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
  QTRY_COMPARE(m_model->sessionActionState.requests.size(), 3);
  QCOMPARE(m_model->sessionActionState.requests.constLast(),
           QStringLiteral("reboot"));
}

void PowerPageTest::routesKeyboardAndProfileActions() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *profile = findItem(page, QStringLiteral("powerProfile_balanced"));
  auto *keyboard = findItem(page,
      QStringLiteral("powerKeyboardBrightness_keyboard-41-1"));
  QVERIFY(profile != nullptr);
  QVERIFY(keyboard != nullptr);
  QVERIFY(QMetaObject::invokeMethod(profile, "clicked"));
  QCOMPARE(m_model->profileCount, 1);
  QCOMPARE(m_model->lastTarget, QStringLiteral("balanced"));
  keyboard->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), keyboard);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(m_model->brightnessCount, 1);
  QCOMPARE(m_model->lastTarget, QStringLiteral("keyboard-41-1"));
  QCOMPARE(m_model->lastNormalized, 5'100);
}

void PowerPageTest::internalSliderStaysResponsiveThroughRepublication() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  const QString sliderName =
      QStringLiteral("powerInternalBrightnessSlider_internal-41-1");
  auto *slider = findItem(page, sliderName);
  QVERIFY2(slider != nullptr, "internal display brightness has no slider");
  QVERIFY(!slider->isEnabled());
  QCOMPARE(m_model->internalBrightnessCount, 0);

  // AGENT-GUARD: republished rows must keep the same delegate. Rebuilding it
  // drops keyboard focus after every step and kills a pointer drag mid-way.
  m_model->setInternalRow(true);
  QCoreApplication::processEvents();
  QCOMPARE(findItem(page, sliderName), slider);
  QVERIFY(slider->isEnabled());
  slider->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), slider);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(m_model->internalBrightnessCount, 1);
  QCOMPARE(m_model->lastTarget, QStringLiteral("internal-41-1"));
  QVERIFY(m_model->lastNormalized > 4'493);

  // An operation fence disables the control; convergence re-enables the same
  // control with keyboard focus intact.
  m_model->setInternalRow(false, 4'600);
  QCoreApplication::processEvents();
  QCOMPARE(findItem(page, sliderName), slider);
  QVERIFY(!slider->isEnabled());
  m_model->setInternalRow(true, 4'600);
  QCoreApplication::processEvents();
  QVERIFY(slider->isEnabled());
  QTRY_COMPARE(m_view->activeFocusItem(), slider);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(m_model->internalBrightnessCount, 2);

  m_model->republishOnInternalRequest = true;
  const qreal y = slider->height() / 2;
  const QPoint start =
      slider->mapToScene(QPointF(slider->width() * 0.2, y)).toPoint();
  const QPoint end =
      slider->mapToScene(QPointF(slider->width() * 0.8, y)).toPoint();
  QVERIFY2(start.y() > 0 && start.y() < m_view->height(),
           "focused internal slider was not revealed in the viewport");
  const int before = m_model->internalBrightnessCount;
  QTest::mousePress(m_view.get(), Qt::LeftButton, Qt::NoModifier, start);
  for (int step = 1; step <= 6; ++step)
    QTest::mouseMove(m_view.get(), start + (end - start) * step / 6);
  QTest::mouseRelease(m_view.get(), Qt::LeftButton, Qt::NoModifier, end);
  QTRY_VERIFY(m_model->internalBrightnessCount >= before + 2);
  QCOMPARE(findItem(page, sliderName), slider);
  QVERIFY(m_model->lastNormalized > 6'000);
}

void PowerPageTest::externalDisplayRowsOfferAdmittedSlidersAndReadOnlyReasons() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto &external = m_model->externalBrightnessState;
  const QString sliderName =
      QStringLiteral("powerExternalBrightnessSlider_external-1");
  QVERIFY(findItem(page, sliderName) == nullptr);
  external.setRows({
      StubExternalBrightness::row(QStringLiteral("external-1"),
          QStringLiteral("Studio monitor"), true, 6'000, {}),
      StubExternalBrightness::row(QStringLiteral("external-2"),
          QStringLiteral("Office TV"), false, 0,
          QStringLiteral("This display does not offer brightness control"))});
  QCoreApplication::processEvents();

  auto *slider = findItem(page, sliderName);
  QVERIFY2(slider != nullptr, "external display brightness has no slider");
  QVERIFY(slider->isVisible());
  QVERIFY(slider->isEnabled());
  QCOMPARE(slider->property("value").toReal(), 6'000.0);
  auto *accessible = QAccessible::queryAccessibleInterface(slider);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::Slider);
  QVERIFY(accessible->text(QAccessible::Name).contains(QStringLiteral("Studio monitor")));
  auto *level = findItem(page, QStringLiteral("powerExternalBrightness_external-1"));
  QVERIFY(level != nullptr);
  QVERIFY(level->property("text").toString().contains(QStringLiteral("60%")));

  // A display without brightness control stays listed with its reason and
  // offers no control.
  auto *readOnly = findItem(page,
      QStringLiteral("powerExternalBrightnessSlider_external-2"));
  QVERIFY(readOnly != nullptr);
  QVERIFY(!readOnly->isVisible());
  QVERIFY(!readOnly->isEnabled());
  auto *reason = findItem(page, QStringLiteral("powerExternalReason_external-2"));
  QVERIFY(reason != nullptr);
  QVERIFY(reason->isVisible());
  QVERIFY(reason->property("text").toString().contains(QStringLiteral("does not offer")));

  slider->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), slider);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(external.requestCount, 1);
  QCOMPARE(external.lastTarget, QStringLiteral("external-1"));
  QCOMPARE(external.lastNormalized, 6'100);

  // A fence disables the same control; failure text stays visible.
  external.errorText = QStringLiteral(
      "The display brightness change could not be confirmed. It was not replayed.");
  external.setAvailable(0, false);
  QCoreApplication::processEvents();
  QCOMPARE(findItem(page, sliderName), slider);
  QVERIFY(!slider->isEnabled());
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QCOMPARE(external.requestCount, 1);
  auto *error = findItem(page, QStringLiteral("powerExternalBrightnessError"));
  QVERIFY(error != nullptr);
  QVERIFY(error->isVisible());
  QVERIFY(error->property("text").toString().contains(QStringLiteral("not replayed")));
}

void PowerPageTest::compactAndUnavailableFocusRemainAdmitted() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *screenLockToggle = findItem(page, QStringLiteral("powerAutomaticScreenLock"));
  QVERIFY(screenLockToggle != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), screenLockToggle);
  QVERIFY(screenLockToggle->isEnabled());

  m_model->ready = false;
  m_model->unavailable = true;
  m_model->profileRows.clear();
  m_model->keyboardBrightnessRows.clear();
  m_model->internalBrightnessRows.clear();
  m_model->supplyRows.clear();
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *retry = findItem(page, QStringLiteral("powerRetryButton"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isEnabled());
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), retry);
  retry->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), retry);
}


void PowerPageTest::screenLockControlsRespectAutomaticLock() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *toggle = findItem(page, QStringLiteral("powerAutomaticScreenLock"));
  auto *selector = findItem(page, QStringLiteral("powerScreenLockTimeoutSelector"));
  QVERIFY(toggle != nullptr);
  QVERIFY(selector != nullptr);
  QVERIFY(!selector->isEnabled());
  QCOMPARE(m_screenLock->timeoutCalls, 0);
  // Qt 6 emits toggled() only for interactive toggles, so drive the Switch
  // with a real click instead of the programmatic toggle() method.
  const QPoint toggleCenter = toggle->mapToScene(
      QPointF(toggle->width() / 2.0, toggle->height() / 2.0)).toPoint();
  QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier, toggleCenter);
  QTRY_COMPARE(m_screenLock->automaticLockCalls, 1);
  QVERIFY(m_screenLock->automaticLock);
  QTRY_VERIFY(selector->isEnabled());

  // A real keyboard selection, rather than a programmatic currentIndex
  // update, is the only path that writes the chosen timeout.
  selector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), selector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_screenLock->timeoutCalls, 1);
  QCOMPARE(m_screenLock->timeoutMinutes, 10);

  m_screenLock->timeoutMinutes = 17;
  Q_EMIT m_screenLock->changed();
  QTRY_COMPARE(selector->property("currentText").toString(), QStringLiteral("17 minutes"));
  QCOMPARE(m_screenLock->timeoutCalls, 1);
}

void PowerPageTest::idleDisplayControlsRespectThePolicy() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *toggle = findItem(page, QStringLiteral("powerIdleDisplayOff"));
  auto *selector =
      findItem(page, QStringLiteral("powerIdleDisplayOffTimeoutSelector"));
  QVERIFY(toggle != nullptr);
  QVERIFY(selector != nullptr);
  QVERIFY(selector->isEnabled());
  QCOMPARE(m_idleDisplay->enabledCalls, 0);

  scrollIntoView(page, toggle);
  const QPoint toggleCenter = toggle->mapToScene(
      QPointF(toggle->width() / 2.0, toggle->height() / 2.0)).toPoint();
  QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier, toggleCenter);
  QTRY_COMPARE(m_idleDisplay->enabledCalls, 1);
  QVERIFY(!m_idleDisplay->enabled);
  QTRY_VERIFY(!selector->isEnabled());

  m_idleDisplay->enabled = true;
  m_idleDisplay->minutes = 15;
  Q_EMIT m_idleDisplay->changed();
  QTRY_VERIFY(selector->isEnabled());
  QTRY_COMPARE(selector->property("currentText").toString(),
               QStringLiteral("15 minutes"));
  QCOMPARE(m_idleDisplay->minutesCalls, 0);
}

void PowerPageTest::resumeLockAndGraceRowsBindAndApply() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *resumeSwitch = findItem(page, QStringLiteral("powerScreenLockOnResume"));
  auto *graceSelector = findItem(page, QStringLiteral("powerScreenLockGraceSelector"));
  QVERIFY(resumeSwitch != nullptr);
  QVERIFY(graceSelector != nullptr);
  QVERIFY(resumeSwitch->isEnabled());
  QCOMPARE(resumeSwitch->property("checked").toBool(), true);
  QVERIFY(graceSelector->isEnabled());
  QCOMPARE(graceSelector->property("currentText").toString(),
           QStringLiteral("30 seconds"));
  QCOMPARE(m_screenLock->lockOnResumeCalls, 0);
  QCOMPARE(m_screenLock->lockGraceCalls, 0);

  // A real click is the only path that writes the resume preference.
  const QPoint resumeCenter = resumeSwitch->mapToScene(
      QPointF(resumeSwitch->width() / 2.0,
              resumeSwitch->height() / 2.0)).toPoint();
  QTest::mouseClick(m_view.get(), Qt::LeftButton, Qt::NoModifier, resumeCenter);
  QTRY_COMPARE(m_screenLock->lockOnResumeCalls, 1);
  QVERIFY(!m_screenLock->lockOnResume);
  QTRY_COMPARE(resumeSwitch->property("checked").toBool(), false);

  // A stored out-of-set grace (an upstream custom delay) stays visible as an
  // extra entry instead of being silently rewritten.
  m_screenLock->lockGraceSeconds = 17;
  Q_EMIT m_screenLock->changed();
  QTRY_COMPARE(graceSelector->property("currentText").toString(),
               QStringLiteral("17 seconds"));
  QCOMPARE(m_screenLock->lockGraceCalls, 0);

  // Keyboard selection writes the chosen grace value.
  graceSelector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), graceSelector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_screenLock->lockGraceCalls, 1);
  QCOMPARE(m_screenLock->lockGraceSeconds, 30);
}

void PowerPageTest::powerPolicyRowsRespectLidPresenceAndApply() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *powerButtonSelector = findItem(page,
      QStringLiteral("powerPowerButtonSelector"));
  auto *lidRow = findItem(page, QStringLiteral("powerLidActionRow"));
  auto *externalRow = findItem(page,
      QStringLiteral("powerLidExternalMonitorRow"));
  QVERIFY(powerButtonSelector != nullptr);
  QVERIFY(lidRow != nullptr);
  QVERIFY(externalRow != nullptr);
  QVERIFY(powerButtonSelector->isVisible());
  QVERIFY(powerButtonSelector->isEnabled());
  // The development machine has no lid: both lid rows must hide.
  QVERIFY(!lidRow->isVisible());
  QVERIFY(!externalRow->isVisible());

  // Admitted lid presence reveals exactly the two lid rows.
  m_model->lidPresent = true;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QTRY_VERIFY(lidRow->isVisible());
  QTRY_VERIFY(externalRow->isVisible());

  auto *lidSelector = findItem(page, QStringLiteral("powerLidActionSelector"));
  auto *externalSwitch = findItem(page,
      QStringLiteral("powerLidExternalMonitorSwitch"));
  QVERIFY(lidSelector != nullptr);
  QVERIFY(externalSwitch != nullptr);
  QCOMPARE(lidSelector->property("currentText").toString(),
           QStringLiteral("Sleep"));
  QCOMPARE(externalSwitch->property("checked").toBool(), false);

  // Keyboard selection on the power-button row writes only that action.
  powerButtonSelector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), powerButtonSelector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_model->lidPolicyState.applyCalls, 1);
  const QVariantList lastApply =
      m_model->lidPolicyState.applyArguments.constLast();
  // applyPolicy(lidAction, wakeWithExternalMonitor, powerButtonAction): the
  // keyboard step changed only the power-button slot (32 -> 64).
  QCOMPARE(lastApply.at(0).toUInt(), 1u);
  QCOMPARE(lastApply.at(1).toBool(), false);
  QCOMPARE(lastApply.at(2).toUInt(), 64u);
}

void PowerPageTest::focusChainReachesTheLidPolicySection() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *powerButtonSelector = findItem(page,
      QStringLiteral("powerPowerButtonSelector"));
  QVERIFY(powerButtonSelector != nullptr);

  // With the screen-lock controls disabled (busy), the page's host-entry
  // target walks to the lid/power-button section's enabled selector.
  m_screenLock->busy = true;
  Q_EMIT m_screenLock->changed();
  QCoreApplication::processEvents();
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(),
           powerButtonSelector);

  powerButtonSelector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), powerButtonSelector);
}

void PowerPageTest::automaticProfileRowsBuildOptionsAndApplyPerSource() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  auto *acSelector = findItem(page,
      QStringLiteral("powerAutoProfileSelector_ac"));
  auto *batterySelector = findItem(page,
      QStringLiteral("powerAutoProfileSelector_battery"));
  auto *lowBatterySelector = findItem(page,
      QStringLiteral("powerAutoProfileSelector_lowBattery"));
  QVERIFY(acSelector != nullptr);
  QVERIFY(batterySelector != nullptr);
  QVERIFY(lowBatterySelector != nullptr);
  QVERIFY(acSelector->isEnabled());

  // No source has an automatic switch configured yet: every selector shows
  // the "don't switch" placeholder, not the sole reported profile.
  QCOMPARE(acSelector->property("currentText").toString(),
           QStringLiteral("Don't switch automatically"));

  // Admitting an AC preference selects it in that row only.
  m_model->profilePolicyState.acProfileId = QStringLiteral("balanced");
  Q_EMIT m_model->profilePolicyState.policyChanged();
  QCoreApplication::processEvents();
  QTRY_COMPARE(acSelector->property("currentText").toString(),
               QStringLiteral("Balanced"));
  QCOMPARE(batterySelector->property("currentText").toString(),
           QStringLiteral("Don't switch automatically"));

  // Selecting the one reported profile on the battery row writes only that
  // source; AC and low-battery are forwarded unchanged.
  batterySelector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), batterySelector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_model->profilePolicyState.applyCalls, 1);
  const QStringList lastApply = m_model->profilePolicyState.lastArguments;
  QCOMPARE(lastApply.at(0), QStringLiteral("balanced"));
  QCOMPARE(lastApply.at(1), QStringLiteral("balanced"));
  QCOMPARE(lastApply.at(2), QString());

  // Busy or unavailable disables all three rows, mirroring the lid section.
  m_model->profilePolicyState.available = false;
  Q_EMIT m_model->profilePolicyState.availabilityChanged();
  QCoreApplication::processEvents();
  QTRY_VERIFY(!acSelector->isEnabled());
  QTRY_VERIFY(!batterySelector->isEnabled());
  QTRY_VERIFY(!lowBatterySelector->isEnabled());
}

QTEST_MAIN(PowerPageTest)
#include "tst_power_page.moc"
