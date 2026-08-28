// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_catalog.h"

#include <QTest>

using namespace QindaQt::Services::FontPreferences;

class tst_FontCatalog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testEmptyFactsRejection();
    void testInvalidFactRejection();
    void testConflictingMonospaceRejection();
    void testNormalizationAndDeduplication();
    void testDeterministicSorting();
    void testMonospaceAndProportionalFiltering();
    void testDefaultFallbackCatalog();
    void testResolveFamily();
};

void tst_FontCatalog::testEmptyFactsRejection()
{
    QString error;
    FontCatalog catalog = FontCatalog::create({}, &error);
    QVERIFY(catalog.isEmpty());
    QCOMPARE(catalog.familyCount(), 0);
    QVERIFY(!error.isEmpty());
}

void tst_FontCatalog::testInvalidFactRejection()
{
    QString error;
    FontFact badFact;
    badFact.family = QStringLiteral("Bad\0Name");
    badFact.style = QStringLiteral("Regular");

    FontCatalog catalog = FontCatalog::create({badFact}, &error);
    QVERIFY(catalog.isEmpty());
    QVERIFY(error.contains(QStringLiteral("invalid")));

    FontFact emptyFact;
    emptyFact.family = QStringLiteral("   ");
    catalog = FontCatalog::create({emptyFact}, &error);
    QVERIFY(catalog.isEmpty());
}

void tst_FontCatalog::testConflictingMonospaceRejection()
{
    QString error;
    QList<FontFact> facts = {
        {QStringLiteral("Hack"), QStringLiteral("Regular"), true, true, 400, false, QString()},
        {QStringLiteral("Hack"), QStringLiteral("Bold"), false, true, 700, false, QString()}
    };

    FontCatalog catalog = FontCatalog::create(facts, &error);
    QVERIFY(catalog.isEmpty());
    QVERIFY(error.contains(QStringLiteral("Conflicting monospace")));
}

void tst_FontCatalog::testNormalizationAndDeduplication()
{
    QList<FontFact> facts = {
        {QStringLiteral("  Inter   Display  "), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("inter display"), QStringLiteral("Bold"), false, true, 700, false, QString()},
        {QStringLiteral("INTER DISPLAY"), QStringLiteral("Italic"), false, true, 400, true, QString()},
        {QStringLiteral("Inter Display"), QStringLiteral("Regular"), false, true, 400, false, QString()}
    };

    QString error;
    FontCatalog catalog = FontCatalog::create(facts, &error);
    QVERIFY(!catalog.isEmpty());
    QCOMPARE(catalog.familyCount(), 1);

    const auto entry = catalog.findFamily(QStringLiteral("Inter Display"));
    QVERIFY(entry.has_value());
    QCOMPARE(entry->familyName, QStringLiteral("Inter Display"));
    QCOMPARE(entry->styles.size(), 3);
    QCOMPARE(entry->styles, QStringList({QStringLiteral("Bold"), QStringLiteral("Italic"), QStringLiteral("Regular")}));
}

void tst_FontCatalog::testDeterministicSorting()
{
    QList<FontFact> facts = {
        {QStringLiteral("Ubuntu"), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("Cantarell"), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("Noto Sans"), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("DejaVu Sans"), QStringLiteral("Regular"), false, true, 400, false, QString()}
    };

    FontCatalog catalog = FontCatalog::create(facts);
    const QStringList names = catalog.familyNames();
    QCOMPARE(names, QStringList({
        QStringLiteral("Cantarell"),
        QStringLiteral("DejaVu Sans"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("Ubuntu")
    }));
}

void tst_FontCatalog::testMonospaceAndProportionalFiltering()
{
    QList<FontFact> facts = {
        {QStringLiteral("Noto Sans"), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("JetBrains Mono"), QStringLiteral("Regular"), true, true, 400, false, QString()},
        {QStringLiteral("Fira Code"), QStringLiteral("Regular"), true, true, 400, false, QString()},
        {QStringLiteral("Roboto"), QStringLiteral("Regular"), false, true, 400, false, QString()}
    };

    FontCatalog catalog = FontCatalog::create(facts);
    QCOMPARE(catalog.familyCount(), 4);
    QCOMPARE(catalog.monospaceFamilyNames(), QStringList({QStringLiteral("Fira Code"), QStringLiteral("JetBrains Mono")}));
    QCOMPARE(catalog.proportionalFamilyNames(), QStringList({QStringLiteral("Noto Sans"), QStringLiteral("Roboto")}));
}

void tst_FontCatalog::testDefaultFallbackCatalog()
{
    FontCatalog catalog = FontCatalog::createDefaultFallback();
    QVERIFY(!catalog.isEmpty());
    QVERIFY(catalog.containsFamily(QStringLiteral("Noto Sans")));
    QVERIFY(catalog.containsFamily(QStringLiteral("Noto Sans Mono")));
    QVERIFY(catalog.containsFamily(QStringLiteral("Sans Serif")));
    QVERIFY(catalog.containsFamily(QStringLiteral("Monospace")));
}

void tst_FontCatalog::testResolveFamily()
{
    FontCatalog catalog = FontCatalog::createDefaultFallback();
    QCOMPARE(catalog.resolveFamily(QStringLiteral("noto sans")), QStringLiteral("Noto Sans"));
    QCOMPARE(catalog.resolveFamily(QStringLiteral("NonExistent"), QStringLiteral("Noto Sans Mono")), QStringLiteral("Noto Sans Mono"));
    QCOMPARE(catalog.resolveFamily(QStringLiteral("NonExistent"), QStringLiteral("AlsoNonExistent")), QStringLiteral("Monospace"));
}

QTEST_MAIN(tst_FontCatalog)
#include "tst_font_catalog.moc"
