// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_printing/application_printing_tool_catalog.h>
#include <qindaqt/apps/settings_printing/printing_settings_model.h>
#include "printing_test_support.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::SettingsPrinting;
using namespace QindaQt::Apps::SettingsPrinting::Test;
namespace {
bool writeDesktop(const QString &root, const QString &id, const QString &extra = {},
                  const QString &exec = QStringLiteral("fixture --start %u")) {
    if (!QDir().mkpath(root + QStringLiteral("/applications"))) return false;
    QFile file(root + QStringLiteral("/applications/") + id + QStringLiteral(".desktop"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto document = (QStringLiteral("[Desktop Entry]\nType=Application\nName=Fixture\nExec=")
                           + exec + QStringLiteral("\nTerminal=false\nCategories=Settings;\n") + extra).toUtf8();
    return file.write(document) == document.size();
}
}
class PrintingToolCatalogTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void fixedOwnersAndPortablePrinterEntry();
    void freshClickSeesRemovalAndReplacement();
    void higherRootMasksAndNoDisplay();
    void terminalDbusAndMalformedEntriesRefuse_data();
    void terminalDbusAndMalformedEntriesRefuse();
    void unsupportedPrimaryCannotFallThrough();
    void literalArgumentsAndNoDocumentHandoff();
    void arbitraryInstalledApplicationIsNeverChosen();
};
void PrintingToolCatalogTest::fixedOwnersAndPortablePrinterEntry() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("Gentoo-system-config-printer")));
    QVERIFY(writeDesktop(root.path(), QStringLiteral("cups")));
    QVERIFY(writeDesktop(root.path(), QStringLiteral("org.gnome.SimpleScan")));
    ApplicationPrintingToolCatalog catalog({root.path()});
    for (const auto &state : catalog.inspect())
        QCOMPARE(state.availability, PrintingToolAvailability::Available);
    QCOMPARE(catalog.prepare(PrintingTool::DocumentScanner).program, QStringLiteral("fixture"));
    QVERIFY(QFile::remove(root.path()+"/applications/Gentoo-system-config-printer.desktop"));
    QVERIFY(writeDesktop(root.path(), QStringLiteral("system-config-printer")));
    QCOMPARE(catalog.prepare(PrintingTool::PrintSettings).state.availability,
             PrintingToolAvailability::Available);
}
void PrintingToolCatalogTest::freshClickSeesRemovalAndReplacement() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("org.gnome.SimpleScan")));
    ApplicationPrintingToolCatalog catalog({root.path()});
    FakePrintingStarter starter;
    PrintingSettingsModel model(catalog, starter);
    QVERIFY(model.tools()[2].toMap().value("available").toBool());
    QVERIFY(QFile::remove(root.path()+"/applications/org.gnome.SimpleScan.desktop"));
    model.launchTool(QStringLiteral("document-scanner"));
    QVERIFY(starter.calls.isEmpty());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("org.gnome.SimpleScan"), {},
                         QStringLiteral("replacement --new")));
    model.launchTool(QStringLiteral("document-scanner"));
    QCOMPARE(starter.calls.size(), 1);
    QCOMPARE(starter.calls[0].first, QStringLiteral("replacement"));
    QCOMPARE(starter.calls[0].second, QStringList{QStringLiteral("--new")});
}
void PrintingToolCatalogTest::higherRootMasksAndNoDisplay() {
    QTemporaryDir high, low;
    QVERIFY(high.isValid());
    QVERIFY(low.isValid());
    QVERIFY(writeDesktop(low.path(), QStringLiteral("cups")));
    QVERIFY(writeDesktop(high.path(), QStringLiteral("cups"), QStringLiteral("Hidden=true\n")));
    ApplicationPrintingToolCatalog catalog({high.path(), low.path()});
    QCOMPARE(catalog.prepare(PrintingTool::CupsAdministration).state.availability,
             PrintingToolAvailability::Missing);
    QVERIFY(writeDesktop(high.path(), QStringLiteral("cups"), QStringLiteral("NoDisplay=true\n")));
    QCOMPARE(catalog.prepare(PrintingTool::CupsAdministration).state.availability,
             PrintingToolAvailability::Missing);
    QFile malformed(high.path()+"/applications/cups.desktop");
    QVERIFY(malformed.open(QIODevice::WriteOnly));
    QVERIFY(malformed.write("[Desktop Entry]\nType=Application\nExec=invalid\n") > 0);
    malformed.close();
    QCOMPARE(catalog.prepare(PrintingTool::CupsAdministration).state.availability,
             PrintingToolAvailability::Missing);
}
void PrintingToolCatalogTest::terminalDbusAndMalformedEntriesRefuse_data() {
    QTest::addColumn<QString>("keys");
    QTest::newRow("terminal") << QStringLiteral("Terminal=true\n");
    QTest::newRow("dbus") << QStringLiteral("DBusActivatable=true\n");
    QTest::newRow("invalid-exec") << QStringLiteral("Exec=broken %Z\n");
}
void PrintingToolCatalogTest::terminalDbusAndMalformedEntriesRefuse() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.path()+"/applications"));
    QFETCH(QString, keys);
    QFile file(root.path()+"/applications/org.gnome.SimpleScan.desktop");
    QVERIFY(file.open(QIODevice::WriteOnly));
    auto text = QStringLiteral("[Desktop Entry]\nType=Application\nName=Fixture\nExec=fixture\n")+keys;
    const auto bytes = text.toUtf8();
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();
    ApplicationPrintingToolCatalog catalog({root.path()});
    const auto result = catalog.prepare(PrintingTool::DocumentScanner);
    QVERIFY(result.state.availability != PrintingToolAvailability::Available);
    QVERIFY(result.program.isEmpty());
}
void PrintingToolCatalogTest::unsupportedPrimaryCannotFallThrough() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("system-config-printer")));
    QFile primary(root.path()+"/applications/Gentoo-system-config-printer.desktop");
    QVERIFY(primary.open(QIODevice::WriteOnly));
    const auto bytes = QByteArray("[Desktop Entry]\nType=Application\nName=Fixture\nExec=fixture\nTerminal=true\n");
    QCOMPARE(primary.write(bytes), bytes.size());
    primary.close();
    ApplicationPrintingToolCatalog catalog({root.path()});
    QCOMPARE(catalog.prepare(PrintingTool::PrintSettings).state.availability,
             PrintingToolAvailability::Unsupported);
}
void PrintingToolCatalogTest::literalArgumentsAndNoDocumentHandoff() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    // Public parser expands only freedesktop field codes, never shell syntax.
    QVERIFY(writeDesktop(root.path(), QStringLiteral("cups"), {},
                         QStringLiteral("xdg-open http://localhost:631/ %u")));
    ApplicationPrintingToolCatalog catalog({root.path()});
    const auto plan = catalog.prepare(PrintingTool::CupsAdministration);
    QCOMPARE(plan.state.availability, PrintingToolAvailability::Available);
    QCOMPARE(plan.program, QStringLiteral("xdg-open"));
    QCOMPARE(plan.arguments, QStringList{QStringLiteral("http://localhost:631/")});
    FakePrintingStarter starter;
    PrintingSettingsModel model(catalog, starter);
    model.launchTool(QStringLiteral("cups-administration"));
    QCOMPARE(starter.calls.size(), 1);
    QCOMPARE(starter.calls[0].second, plan.arguments);
    QVERIFY(writeDesktop(root.path(), QStringLiteral("cups"), {},
                         QStringLiteral("fixture ; echo literal | cat `id` $(id) %u")));
    model.launchTool(QStringLiteral("cups-administration"));
    QCOMPARE(starter.calls.size(), 2);
    QCOMPARE(starter.calls[1].second, QStringList({
        QStringLiteral(";"), QStringLiteral("echo"), QStringLiteral("literal"),
        QStringLiteral("|"), QStringLiteral("cat"), QStringLiteral("`id`"),
        QStringLiteral("$(id)")}));
}
void PrintingToolCatalogTest::arbitraryInstalledApplicationIsNeverChosen() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("some-printer")));
    ApplicationPrintingToolCatalog catalog({root.path()});
    for (const auto &state : catalog.inspect())
        QCOMPARE(state.availability, PrintingToolAvailability::Missing);
}
QTEST_GUILESS_MAIN(PrintingToolCatalogTest)
#include "tst_printing_tool_catalog.moc"
