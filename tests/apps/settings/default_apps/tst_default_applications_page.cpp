// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_default_apps/default_applications_settings_model.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationPreferences;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsSettingsModel;
using QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsStore;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}

class TestStore final : public DefaultApplicationsStore {
public:
  DefaultApplicationPreferences preferences;
  QList<DefaultApplicationPreferences> saves;
  bool load(DefaultApplicationPreferences *out, QString *error) override {
    Q_UNUSED(error);
    *out = preferences;
    return true;
  }
  bool save(const DefaultApplicationPreferences &value, QString *error) override {
    Q_UNUSED(error);
    saves.append(value);
    preferences = value;
    return true;
  }
};

DirectoryScan makeScan() {
  DirectoryScan scan;
  ScannedApplication browser;
  browser.entry.id = QStringLiteral("userapp-QindaFox.desktop");
  browser.entry.name = QStringLiteral("QindaFox");
  browser.documentText = QStringLiteral("[Desktop Entry]\nMimeType=text/html;\n");
  scan.applications = {browser};
  return scan;
}
} // namespace

class DefaultApplicationsPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersOneRowPerCategoryWithCurrentSelection();
  void selectingAnOptionCallsSetDefaultApplication();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<DefaultApplicationsSettingsModel> m_model;
  TestStore *m_store = nullptr;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void DefaultApplicationsPageTest::initTestCase() {
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
DefaultApplicationsPageTest::createPage(const QSize size) {
  auto storeOwned = std::make_unique<TestStore>();
  storeOwned->preferences.browser = QStringLiteral("userapp-QindaFox.desktop");
  m_store = storeOwned.get();
  m_model = std::make_unique<DefaultApplicationsSettingsModel>(
      std::move(storeOwned), makeScan());
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_DEFAULT_APPLICATIONS_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("defaultApplicationsSettings"),
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

void DefaultApplicationsPageTest::rendersOneRowPerCategoryWithCurrentSelection() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *browserSelector =
      findItem(page, QStringLiteral("defaultApplicationSelector_browser"));
  auto *fileManagerSelector =
      findItem(page, QStringLiteral("defaultApplicationSelector_file-manager"));
  QVERIFY(browserSelector != nullptr);
  QVERIFY(fileManagerSelector != nullptr);
  QCOMPARE(browserSelector->property("currentText").toString(),
           QStringLiteral("QindaFox"));
  QCOMPARE(fileManagerSelector->property("currentText").toString(),
           QStringLiteral("None"));
}

void DefaultApplicationsPageTest::selectingAnOptionCallsSetDefaultApplication() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *browserSelector =
      findItem(page, QStringLiteral("defaultApplicationSelector_browser"));
  QVERIFY(browserSelector != nullptr);

  // Current selection is index 1 ("QindaFox"); Up moves to index 0 ("None"),
  // which must save an empty (unset) browser default.
  browserSelector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), browserSelector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_store->saves.size(), 1);
  QCOMPARE(m_store->saves.constFirst().browser, QString());
}

QTEST_MAIN(DefaultApplicationsPageTest)
#include "tst_default_applications_page.moc"
