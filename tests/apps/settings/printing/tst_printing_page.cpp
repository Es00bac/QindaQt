// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_printing/printing_settings_model.h>
#include <qindaqt/themes/theme_loader.h>
#include "printing_test_support.h"
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>
#include <memory>

using namespace QindaQt::Apps::SettingsPrinting;
using namespace QindaQt::Apps::SettingsPrinting::Test;
namespace {
QQuickItem *item(QQuickItem *root, const QString &name) {
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = item(child, name)) return found;
    return nullptr;
}
}
class PrintingPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void compactAndNormalLayout_data();
    void compactAndNormalLayout();
    void keyboardSkipsMissingAndActivatesOnce();
    void missingToolsRefreshWithoutLaunching();
    void ownerLabelsAndLaunchStatusStayPlain();
private:
    FakePrintingCatalog m_catalog;
    FakePrintingStarter m_starter;
    std::unique_ptr<PrintingSettingsModel> m_model;
    std::unique_ptr<QQuickView> m_view;
    std::unique_ptr<QObject> m_page;
    QQuickItem *createPage(QSize size);
};
void PrintingPageTest::init() {
    m_catalog = {};
    m_starter = {};
    m_view = std::make_unique<QQuickView>();
    m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*m_view->engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));
    if (qEnvironmentVariable("QT_SCALE_FACTOR") == QStringLiteral("2"))
        QCOMPARE(m_view->devicePixelRatio(), 2.0);
}
void PrintingPageTest::cleanup() {
    m_page.reset();
    m_view.reset();
    m_model.reset();
}
QQuickItem *PrintingPageTest::createPage(const QSize size) {
    m_model = std::make_unique<PrintingSettingsModel>(m_catalog, m_starter);
    QQmlComponent component(m_view->engine());
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(QINDAQT_PRINTING_PAGE_QML_PATH)));
    if (!component.isReady()) {
        qWarning().noquote() << component.errorString();
        return nullptr;
    }
    m_page.reset(component.createWithInitialProperties({
        {QStringLiteral("printingSettings"), QVariant::fromValue(static_cast<QObject *>(m_model.get()))}}));
    auto *page = qobject_cast<QQuickItem *>(m_page.get());
    if (page == nullptr) return nullptr;
    m_view->resize(size);
    page->setParentItem(m_view->contentItem());
    page->setSize(size);
    m_view->show();
    m_view->requestActivate();
    QCoreApplication::processEvents();
    return page;
}
void PrintingPageTest::compactAndNormalLayout_data() {
    QTest::addColumn<QSize>("size");
    QTest::newRow("compact") << QSize(420, 320);
    QTest::newRow("normal") << QSize(900, 700);
}
void PrintingPageTest::compactAndNormalLayout() {
    QFETCH(QSize, size);
    m_catalog.available(PrintingTool::PrintSettings);
    m_catalog.available(PrintingTool::CupsAdministration);
    m_catalog.available(PrintingTool::DocumentScanner);
    auto *page = createPage(size);
    QVERIFY(page != nullptr);
    for (const auto tool : {PrintingTool::PrintSettings, PrintingTool::CupsAdministration,
                           PrintingTool::DocumentScanner}) {
        auto *button = item(page, printingToolId(tool)+QStringLiteral("LaunchButton"));
        QVERIFY(button != nullptr);
        QVERIFY(button->isEnabled());
        button->forceActiveFocus();
        QCoreApplication::processEvents();
        const auto top = button->mapToItem(page, QPointF{});
        QVERIFY(top.x() >= 0);
        QVERIFY(top.x()+button->width() <= page->width()+1);
        QVERIFY(top.y() >= 0);
        QVERIFY(top.y()+button->height() <= page->height()+1);
    }
    QVERIFY(m_starter.calls.isEmpty());
}
void PrintingPageTest::keyboardSkipsMissingAndActivatesOnce() {
    m_catalog.available(PrintingTool::CupsAdministration);
    m_catalog.available(PrintingTool::DocumentScanner);
    auto *page = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *cups = item(page, QStringLiteral("cups-administrationLaunchButton"));
    auto *scanner = item(page, QStringLiteral("document-scannerLaunchButton"));
    auto *refresh = item(page, QStringLiteral("printingRefreshButton"));
    QVERIFY(cups != nullptr && scanner != nullptr && refresh != nullptr);
    auto *first = page->property("firstFocusTarget").value<QQuickItem *>();
    QCOMPARE(first, cups);
    first->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QCOMPARE(m_starter.calls.size(), 1);
    QTest::keyClick(m_view.get(), Qt::Key_Tab);
    QTRY_VERIFY(scanner->hasActiveFocus());
    QTest::keyClick(m_view.get(), Qt::Key_Return);
    QCOMPARE(m_starter.calls.size(), 2);
    QTest::keyClick(m_view.get(), Qt::Key_Tab);
    QTRY_VERIFY(refresh->hasActiveFocus());
    QTest::keyClick(m_view.get(), Qt::Key_Tab);
    QTRY_VERIFY(cups->hasActiveFocus());
    QTest::keyClick(m_view.get(), Qt::Key_Backtab);
    QTRY_VERIFY(refresh->hasActiveFocus());
}
void PrintingPageTest::missingToolsRefreshWithoutLaunching() {
    auto *page = createPage(QSize(420, 320));
    QVERIFY(page != nullptr);
    auto *refresh = item(page, QStringLiteral("printingRefreshButton"));
    auto *scanner = item(page, QStringLiteral("document-scannerLaunchButton"));
    QVERIFY(refresh != nullptr && scanner != nullptr);
    QVERIFY(!scanner->isEnabled());
    QCOMPARE(page->property("firstFocusTarget").value<QQuickItem *>(), refresh);
    m_catalog.available(PrintingTool::DocumentScanner);
    refresh->forceActiveFocus();
    QTest::keyClick(m_view.get(), Qt::Key_Space);
    QTRY_VERIFY(scanner->isEnabled());
    QVERIFY(m_starter.calls.isEmpty());
    QTest::keyClick(m_view.get(), Qt::Key_Tab);
    QTRY_VERIFY(scanner->hasActiveFocus());
}
void PrintingPageTest::ownerLabelsAndLaunchStatusStayPlain() {
    m_catalog.available(PrintingTool::DocumentScanner);
    m_catalog.inventory[2].applicationName = QStringLiteral("<b>Scanner & owner</b>");
    auto *page = createPage(QSize(900, 700));
    QVERIFY(page != nullptr);
    auto *label = item(page, QStringLiteral("document-scannerAvailability"));
    QVERIFY(label != nullptr);
    QVERIFY(label->property("text").toString().contains(QStringLiteral("<b>Scanner & owner</b>")));
    QCOMPARE(label->property("textFormat").toInt(), static_cast<int>(Qt::PlainText));
    m_starter.submitted = false;
    m_model->launchTool(QStringLiteral("document-scanner"));
    auto *status = item(page, QStringLiteral("printingLaunchStatus"));
    QVERIFY(status != nullptr);
    QVERIFY(status->property("text").toString().contains(QStringLiteral("could not be started")));
    QCOMPARE(status->property("textFormat").toInt(), static_cast<int>(Qt::PlainText));
}
QTEST_MAIN(PrintingPageTest)
#include "tst_printing_page.moc"
