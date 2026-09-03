// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_power_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsPower::TestSupport::StubPowerSettingsModel;

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
  void sessionActionsHaveKeyboardParityAndDestructiveConfirmation();
  void compactAndUnavailableFocusRemainAdmitted();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubPowerSettingsModel> m_model;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
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
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("powerSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
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
  QVERIFY(!internal->isEnabled());
  QVERIFY(keyboard->isEnabled());
  QVERIFY(raw->property("text").toString().contains(QStringLiteral("421 of 937")));
  auto *sliderAccessible = QAccessible::queryAccessibleInterface(keyboard);
  QVERIFY(sliderAccessible != nullptr);
  QCOMPARE(sliderAccessible->role(), QAccessible::Slider);
  QVERIFY(sliderAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("10000")));
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

void PowerPageTest::compactAndUnavailableFocusRemainAdmitted() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *profile = findItem(page, QStringLiteral("powerProfile_balanced"));
  QVERIFY(profile != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), profile);
  QVERIFY(profile->isEnabled());

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

QTEST_MAIN(PowerPageTest)
#include "tst_power_page.moc"
