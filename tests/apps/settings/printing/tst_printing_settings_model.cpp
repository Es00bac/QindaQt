// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_printing/printing_settings_model.h>
#include "printing_test_support.h"
#include <QtTest>

using namespace QindaQt::Apps::SettingsPrinting;
using namespace QindaQt::Apps::SettingsPrinting::Test;
class PrintingSettingsModelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void constructionAndRefreshNeverLaunch();
    void closedActionsSubmitFreshPlan();
    void metadataLossBeforeClickRefuses();
    void launchFailureAndRefreshRemainTruthful();
    void unknownIntentAndInvalidPlanRefuse();
    void reentrantDispatchIsSingleFlight();
};
void PrintingSettingsModelTest::constructionAndRefreshNeverLaunch() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    PrintingSettingsModel model(catalog, starter);
    QCOMPARE(model.tools().size(), 3);
    QVERIFY(starter.calls.isEmpty());
    QVERIFY(!model.tools()[2].toMap().value("available").toBool());
    QVERIFY(!model.tools()[2].toMap().value("reason").toString().isEmpty());
    catalog.available(PrintingTool::DocumentScanner);
    model.refresh();
    QVERIFY(model.tools()[2].toMap().value("available").toBool());
    QVERIFY(starter.calls.isEmpty());
    QVERIFY(model.statusText().isEmpty());
}
void PrintingSettingsModelTest::closedActionsSubmitFreshPlan() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    PrintingSettingsModel model(catalog, starter);
    for (const auto tool : {PrintingTool::PrintSettings, PrintingTool::CupsAdministration,
                           PrintingTool::DocumentScanner}) {
        catalog.available(tool);
        model.launchTool(printingToolId(tool));
        QCOMPARE(starter.calls.constLast().second, QStringList{printingToolId(tool)});
    }
    QCOMPARE(catalog.preparationsRead, 3);
    QCOMPARE(starter.calls.size(), 3);
    QVERIFY(model.statusText().startsWith("Launch requested"));
    QVERIFY(model.statusText().contains("shown in the application"));
    QVERIFY(!model.busy());
}
void PrintingSettingsModelTest::metadataLossBeforeClickRefuses() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    catalog.available(PrintingTool::PrintSettings);
    PrintingSettingsModel model(catalog, starter);
    QVERIFY(model.tools()[0].toMap().value("available").toBool());
    catalog.available(PrintingTool::PrintSettings, false);
    model.launchTool(QStringLiteral("print-settings"));
    QVERIFY(starter.calls.isEmpty());
    QVERIFY(!model.tools()[0].toMap().value("available").toBool());
    QVERIFY(!model.statusText().isEmpty());
}
void PrintingSettingsModelTest::launchFailureAndRefreshRemainTruthful() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    catalog.available(PrintingTool::DocumentScanner);
    starter.submitted = false;
    PrintingSettingsModel model(catalog, starter);
    model.launchTool(QStringLiteral("document-scanner"));
    QVERIFY(model.statusText().contains("could not be started"));
    starter.submitted = true;
    model.refresh();
    QCOMPARE(starter.calls.size(), 1); // Refresh never retries the previous action.
    QVERIFY(model.statusText().isEmpty());
    model.launchTool(QStringLiteral("document-scanner"));
    QCOMPARE(starter.calls.size(), 2);
    QVERIFY(model.statusText().startsWith("Launch requested"));
}
void PrintingSettingsModelTest::unknownIntentAndInvalidPlanRefuse() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    catalog.available(PrintingTool::PrintSettings);
    PrintingSettingsModel model(catalog, starter);
    for (const auto &id : {QStringLiteral("../cups"), QStringLiteral("/usr/bin/lp"),
                           QStringLiteral("print-settings;echo"), QStringLiteral("cups")})
        model.launchTool(id);
    QCOMPARE(catalog.preparationsRead, 0);
    auto &prepared = catalog.preparations[0];
    prepared.program = QString(4097, QLatin1Char('a'));
    model.launchTool(QStringLiteral("print-settings"));
    QVERIFY(starter.calls.isEmpty());
    QVERIFY(!model.tools()[0].toMap().value("available").toBool());
    prepared.program = QStringLiteral("/fixture/owner");
    prepared.arguments = {QString(QChar::Null)};
    model.launchTool(QStringLiteral("print-settings"));
    QVERIFY(starter.calls.isEmpty());
}
void PrintingSettingsModelTest::reentrantDispatchIsSingleFlight() {
    FakePrintingCatalog catalog;
    FakePrintingStarter starter;
    catalog.available(PrintingTool::PrintSettings);
    PrintingSettingsModel model(catalog, starter);
    starter.duringStart = [&] {
        QVERIFY(model.busy());
        model.launchTool(QStringLiteral("print-settings"));
        model.refresh();
    };
    model.launchTool(QStringLiteral("print-settings"));
    QCOMPARE(starter.calls.size(), 1);
    QCOMPARE(catalog.preparationsRead, 1);
    QCOMPARE(catalog.inspections, 1);
    QVERIFY(!model.busy());
}
QTEST_GUILESS_MAIN(PrintingSettingsModelTest)
#include "tst_printing_settings_model.moc"
