// SPDX-License-Identifier: GPL-3.0-or-later
#include "print_conversion.h"
#include "print_job.h"
#include <QFile>
#include <QBuffer>
#include <QPrintEngine>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
// The public custom-engine API borrows both engines. Keep the independent
// PDF storage alive beyond the wrapper and tested QPrinter, and intercept only
// the CUPS/name properties unsupported by an offline PDF engine.
// https://doc.qt.io/qt-6/qprinter.html#setEngines
class FixtureEngine final : public QPrintEngine {
public:
    explicit FixtureEngine(QPrintEngine &storage) : m_storage(storage) {}
    void setProperty(PrintEnginePropertyKey key, const QVariant &value) override {
        if (key == PrintEnginePropertyKey(0xfe00)) cups = value;
        else if (key == PPK_PrinterName) name = value;
        else if (key == PPK_Duplex || key == PPK_PageOrder || key == PPK_CollateCopies) hardware[key] = value;
        else m_storage.setProperty(key, value);
    }
    QVariant property(PrintEnginePropertyKey key) const override {
        if (key == PrintEnginePropertyKey(0xfe00)) return cups;
        if (key == PPK_PrinterName) return name;
        if (key == PPK_SupportsMultipleCopies) return true;
        if (hardware.contains(key)) return hardware.value(key);
        return m_storage.property(key);
    }
    bool newPage() override { return false; }
    bool abort() override { return true; }
    int metric(QPaintDevice::PaintDeviceMetric key) const override { return m_storage.metric(key); }
    QPrinter::PrinterState printerState() const override { return QPrinter::Idle; }
private:
    QMap<int, QVariant> hardware{{PPK_Duplex, QPrinter::DuplexNone}, {PPK_PageOrder, QPrinter::FirstPageFirst}, {PPK_CollateCopies, false}};
    QPrintEngine &m_storage; QVariant cups = QStringList{}, name = QStringLiteral("fixture");
};
class FixturePrinter final : public QPrinter {
public:
    FixturePrinter(QPrintEngine &engine, QPaintEngine &paint) { setEngines(&engine, &paint); }
};
struct PrintFixture {
    QPrinter storage;
    FixtureEngine engine{*storage.printEngine()};
    FixturePrinter printer{engine, *storage.paintEngine()};
};
class PrintConversionTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void settingsAndPagesRoundTrip() {
        PrintFixture fixture; auto &printer = fixture.printer;
        loadPrintSettings(&printer, {{"n-copies", "3"}, {"resolution", "600"}, {"orientation", "landscape"}, {"use-color", "no"}, {"duplex", "vertical"}, {"collate", "yes"}, {"reverse", "yes"}, {"print-pages", "ranges"}, {"page-ranges", "1-3,7-9"}, {"page-set", "odd"}, {"number-up", "4"}, {"number-up-layout", "rltb"}}, {{"PPDName", "A4"}, {"MarginTop", 10.0}, {"MarginBottom", 11.0}, {"MarginLeft", 12.0}, {"MarginRight", 13.0}});
        QCOMPARE(printer.copyCount(), 3); QCOMPARE(printer.resolution(), 600); QCOMPARE(printer.colorMode(), QPrinter::GrayScale); QCOMPARE(printer.pageLayout().orientation(), QPageLayout::Landscape); QCOMPARE(printer.fromPage(), 1); QCOMPARE(printer.toPage(), 9);
        const auto result = savePrintSettings(&printer); const auto settings = result.value("settings").toObject();
        QCOMPARE(settings.value("duplex").toString(), QStringLiteral("vertical")); QCOMPARE(settings.value("reverse").toString(), QStringLiteral("yes")); QCOMPARE(settings.value("collate").toString(), QStringLiteral("yes")); QCOMPARE(settings.value("n-copies").toString(), QStringLiteral("3")); QCOMPARE(settings.value("number-up").toString(), QStringLiteral("4")); QCOMPARE(settings.value("number-up-layout").toString(), QStringLiteral("rltb"));
        QCOMPARE(result.value("page-setup").toObject().value("PPDName").toString(), QStringLiteral("A4"));
        QCOMPARE(printPageId("EnvC5"), QPageSize::C5E); QCOMPARE(printPageKey(QPageSize::EnvelopeYou4), QStringLiteral("EnvYou4"));
    }
    void argumentsPreservePreformattedPdf() {
        PrintFixture fixture; auto &printer = fixture.printer; printer.setPrinterName("fixture"); printer.setCopyCount(2); printer.setDocName("Fixture job"); printer.setDuplex(QPrinter::DuplexLongSide); printer.setFromTo(2, 4);
        const auto args = PrintArguments::arguments(&printer, true, "lp", QPageLayout::Portrait);
        QVERIFY(args.contains("-d")); QVERIFY(args.contains("fixture")); QVERIFY(args.contains("sides=two-sided-long-edge")); QCOMPARE(args.last(), QStringLiteral("--"));
        // Frontend already applies page ranges/number-up to the PDF.
        QVERIFY(!args.contains("-P")); QVERIFY(!args.contains("page-ranges=2-4"));
    }
    void realRunnerChecksExitStatusWithoutPrinter() {
        QByteArray bytes("fixture"); QBuffer input(&bytes); QVERIFY(input.open(QIODevice::ReadOnly));
        QVERIFY(runPrintCommand(QStringLiteral("/bin/cat"), {}, input));
        QVERIFY(input.seek(0)); QVERIFY(!runPrintCommand(QStringLiteral("/bin/false"), {}, input));
        QVERIFY(input.seek(0)); QVERIFY(!runPrintCommand(QStringLiteral("/does/not/exist"), {}, input));
    }
    void injectedSpoolAndSaveFileErrors() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); QTemporaryFile input; QVERIFY(input.open()); const QByteArray data("%PDF-1.7\nfixture bytes"); QCOMPARE(input.write(data), data.size()); input.flush();
        QVERIFY(lseek(input.handle(), 4, SEEK_SET) == 4);
        PrintFixture fixture; auto &printer = fixture.printer; printer.setOutputFileName(temp.filePath("result.pdf"));
        QVERIFY(submitPrint(printer, input.handle(), [](auto &, auto &, auto &) { return false; }));
        QFile result(temp.filePath("result.pdf")); QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(), data); QCOMPARE(lseek(input.handle(), 0, SEEK_CUR), off_t(4));
        printer.setOutputFileName(temp.filePath("missing/result.pdf")); QVERIFY(!submitPrint(printer, input.handle(), runPrintCommand));
        // Inject only the spool runner; a temporary executable name exercises
        // actual selection/argument/input flow without contacting a printer.
        QFile fake(temp.filePath("lp")); QVERIFY(fake.open(QIODevice::WriteOnly)); fake.write("fixture"); fake.close(); QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto oldPath = qgetenv("PATH"); qputenv("PATH", temp.path().toLocal8Bit()); printer.setOutputFileName({}); printer.setPrinterName("fixture");
        bool called = false;
        QVERIFY(submitPrint(printer, input.handle(), [&](const QString &exe, const QStringList &args, QIODevice &stream) { called = true; return exe == temp.filePath("lp") && args.contains("fixture") && stream.readAll() == data; }));
        QVERIFY(called); QVERIFY(!submitPrint(printer, input.handle(), [](auto &, auto &, auto &) { return false; })); qputenv("PATH", oldPath);
    }
};
QTEST_MAIN(PrintConversionTest)
#include "tst_print_conversion.moc"
