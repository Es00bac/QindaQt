// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_color_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsColor::TestSupport::StubColorSettingsModel;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}
} // namespace

class ColorPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersWideTruthAndAccessibleControls();
  void routesAssignmentActions();
  void compactAndUnavailableFocusRemainAdmitted();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubColorSettingsModel> m_model;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void ColorPageTest::initTestCase() {
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
ColorPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubColorSettingsModel>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_COLOR_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("colorSettings"),
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

void ColorPageTest::rendersWideTruthAndAccessibleControls() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *purpose = findItem(page, QStringLiteral("colorPagePurpose"));
  auto *output = findItem(page, QStringLiteral("colorOutput_edid:dp1"));
  auto *profile = findItem(page, QStringLiteral("colorProfile_vendor-srgb"));
  auto *import = findItem(page, QStringLiteral("colorImportButton"));
  auto *summary = findItem(page, QStringLiteral("colorCatalogSummary"));
  QVERIFY(purpose != nullptr);
  QVERIFY(output != nullptr);
  QVERIFY(profile != nullptr);
  QVERIFY(import != nullptr);
  QVERIFY(summary != nullptr);
  QVERIFY(output->isEnabled());
  QVERIFY(profile->isEnabled());
  QVERIFY(import->isEnabled());
  auto *outputAccessible = QAccessible::queryAccessibleInterface(output);
  QVERIFY(outputAccessible != nullptr);
  QCOMPARE(outputAccessible->role(), QAccessible::RadioButton);
  QVERIFY(outputAccessible->state().checked);
  QVERIFY(outputAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("Main Monitor")));
  auto *profileAccessible = QAccessible::queryAccessibleInterface(profile);
  QVERIFY(profileAccessible != nullptr);
  QVERIFY(profileAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("Assign vendor-srgb")));
  auto *purposeAccessible = QAccessible::queryAccessibleInterface(purpose);
  QVERIFY(purposeAccessible != nullptr);
  QVERIFY(purposeAccessible->text(QAccessible::Name)
              .contains(QStringLiteral("apply an ICC color profile")));
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), profile);
}

void ColorPageTest::routesAssignmentActions() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *output = findItem(page, QStringLiteral("colorOutput_edid:dp1"));
  auto *profile = findItem(page, QStringLiteral("colorProfile_vendor-srgb"));
  QVERIFY(output != nullptr);
  QVERIFY(profile != nullptr);
  QVERIFY(QMetaObject::invokeMethod(output, "clicked"));
  QCOMPARE(m_model->lastOutputId, QStringLiteral("edid:dp1"));
  QVERIFY(QMetaObject::invokeMethod(profile, "clicked"));
  QCOMPARE(m_model->assignCount, 1);
  QCOMPARE(m_model->lastProfileId, QStringLiteral("vendor-srgb"));
}

void ColorPageTest::compactAndUnavailableFocusRemainAdmitted() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *profile = findItem(page, QStringLiteral("colorProfile_vendor-srgb"));
  QVERIFY(profile != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), profile);
  profile->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), profile);

  m_model->ready = false;
  m_model->unavailable = true;
  m_model->outputRows.clear();
  m_model->profileRows.clear();
  m_model->selectedOutputId.clear();
  m_model->selectedOutputName.clear();
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *retry = findItem(page, QStringLiteral("colorRetryButton"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isEnabled());
  // Unavailable truth nominates Retry, never a disabled domain control.
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), retry);
  retry->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), retry);

  // Ready truth without any domain rows falls back to the always-admitted
  // Import button.
  m_model->ready = true;
  m_model->unavailable = false;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *import = findItem(page, QStringLiteral("colorImportButton"));
  QVERIFY(import != nullptr);
  QVERIFY(import->isEnabled());
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), import);
  import->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), import);
}

QTEST_MAIN(ColorPageTest)
#include "tst_color_page.moc"
