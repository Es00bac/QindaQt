// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_about_computer/about_computer_settings_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsAboutComputer::AboutComputerInfo;
using QindaQt::Apps::SettingsAboutComputer::AboutComputerInfoSource;
using QindaQt::Apps::SettingsAboutComputer::AboutComputerSettingsModel;
using QindaQt::Apps::SettingsAboutComputer::BatteryChargeState;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}

class FakeInfoSource final : public AboutComputerInfoSource {
public:
  AboutComputerInfo info;
  AboutComputerInfo read() const override { return info; }
};
} // namespace

class AboutComputerPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersEveryAvailableSection();
  void copyReportButtonSetsClipboardAndStatus();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<AboutComputerSettingsModel> m_model;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void AboutComputerPageTest::initTestCase() {
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
AboutComputerPageTest::createPage(const QSize size) {
  auto sourceOwned = std::make_unique<FakeInfoSource>();
  sourceOwned->info.qindaqtVersion = QStringLiteral("0.1.0");
  sourceOwned->info.hostnamedAvailable = true;
  sourceOwned->info.hostname = QStringLiteral("qinda-top");
  sourceOwned->info.hardwareVendor = QStringLiteral("Lenovo");
  sourceOwned->info.hardwareModel = QStringLiteral("IdeaPad Slim 3 15ABR8");
  sourceOwned->info.chassis = QStringLiteral("laptop");
  sourceOwned->info.kernelName = QStringLiteral("Linux");
  sourceOwned->info.kernelRelease = QStringLiteral("6.18.48-gentoo-dist-bin");
  sourceOwned->info.operatingSystemPrettyName = QStringLiteral("Gentoo Linux");
  sourceOwned->info.diskAvailable = true;
  sourceOwned->info.diskTotalBytes = 500'000'000'000;
  sourceOwned->info.diskAvailableBytes = 250'000'000'000;
  sourceOwned->info.memoryAvailable = true;
  sourceOwned->info.memoryTotalBytes = 16'000'000'000;
  sourceOwned->info.memoryAvailableBytes = 8'000'000'000;
  sourceOwned->info.batteryPresent = true;
  sourceOwned->info.batteryVendor = QStringLiteral("CSMX202");
  sourceOwned->info.batteryModel = QStringLiteral("L22X3PF2");
  sourceOwned->info.batteryPercentageKnown = true;
  sourceOwned->info.batteryPercentage = 98.0;
  sourceOwned->info.batteryState = BatteryChargeState::FullyCharged;
  sourceOwned->info.batteryHealthKnown = true;
  sourceOwned->info.batteryHealthPercent = 99.0;
  m_model = std::make_unique<AboutComputerSettingsModel>(std::move(sourceOwned));

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_ABOUT_COMPUTER_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("aboutComputerSettings"),
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

void AboutComputerPageTest::rendersEveryAvailableSection() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *hostname = findItem(page, QStringLiteral("aboutComputerHostname"));
  auto *hardware = findItem(page, QStringLiteral("aboutComputerHardware"));
  auto *disk = findItem(page, QStringLiteral("aboutComputerDisk"));
  auto *memory = findItem(page, QStringLiteral("aboutComputerMemory"));
  auto *battery = findItem(page, QStringLiteral("aboutComputerBattery"));
  auto *batteryHealth =
      findItem(page, QStringLiteral("aboutComputerBatteryHealth"));
  QVERIFY(hostname != nullptr);
  QVERIFY(hardware != nullptr);
  QVERIFY(disk != nullptr);
  QVERIFY(memory != nullptr);
  QVERIFY(battery != nullptr);
  QVERIFY(batteryHealth != nullptr);
  QVERIFY(hostname->property("text").toString().contains(QStringLiteral("qinda-top")));
  QVERIFY(battery->property("text").toString().contains(QStringLiteral("CSMX202")));
  QVERIFY(batteryHealth->property("text").toString().contains(QStringLiteral("99")));
}

void AboutComputerPageTest::copyReportButtonSetsClipboardAndStatus() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard == nullptr) QSKIP("no clipboard available in this environment");

  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  auto *copyButton =
      findItem(page, QStringLiteral("aboutComputerCopyReportButton"));
  auto *status = findItem(page, QStringLiteral("aboutComputerCopyStatus"));
  QVERIFY(copyButton != nullptr);
  QVERIFY(status != nullptr);

  QMetaObject::invokeMethod(copyButton, "clicked");
  QCoreApplication::processEvents();
  QVERIFY(clipboard->text().contains(QStringLiteral("qinda-top")));
  QVERIFY(status->property("text").toString().length() > 0);
}

QTEST_MAIN(AboutComputerPageTest)
#include "tst_about_computer_page.moc"
