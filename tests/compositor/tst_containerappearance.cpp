// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/containerappearance.h"

#include <QtTest>

using namespace QindaQt::Compositor;

class ContainerAppearanceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void trimsAndAcceptsAnOrdinaryName();
    void rejectsControlAndFormatCharactersInName();
    void rejectsNameOverTheLengthLimit();
    void acceptsAndUppercasesValidHexColors();
    void rejectsMalformedColors();
    void swatchesAreUniqueValidNonDefaultColors();
};

void ContainerAppearanceTests::trimsAndAcceptsAnOrdinaryName()
{
    QCOMPARE(normalizedContainerName(QStringLiteral("  Research Stack  ")),
             QStringLiteral("Research Stack"));
    QVERIFY(normalizedContainerName(QStringLiteral("   ")).isEmpty());
    QVERIFY(normalizedContainerName(QString{}).isEmpty());
}

void ContainerAppearanceTests::rejectsControlAndFormatCharactersInName()
{
    QVERIFY(normalizedContainerName(QStringLiteral("bad\tname")).isEmpty());
    QVERIFY(normalizedContainerName(QStringLiteral("bad\nname")).isEmpty());
    QVERIFY(normalizedContainerName(QString(QChar(0x0000))).isEmpty());
    // A right-to-left override is Other_Format and hostile presentation text.
    QVERIFY(normalizedContainerName(QStringLiteral("bad") + QChar(0x202E) + QStringLiteral("name"))
                .isEmpty());
}

void ContainerAppearanceTests::rejectsNameOverTheLengthLimit()
{
    const QString tooLong(ContainerNameMaximumCharacters + 1, QLatin1Char('a'));
    QVERIFY(normalizedContainerName(tooLong).isEmpty());
    const QString exactly(ContainerNameMaximumCharacters, QLatin1Char('a'));
    QCOMPARE(normalizedContainerName(exactly), exactly);
}

void ContainerAppearanceTests::acceptsAndUppercasesValidHexColors()
{
    QVERIFY(isValidContainerColor(QStringLiteral("#0091FF")));
    QVERIFY(!isValidContainerColor(QStringLiteral("#0091ff")));
    QCOMPARE(normalizedContainerColor(QStringLiteral(" #0091ff ")),
             QStringLiteral("#0091FF"));
}

void ContainerAppearanceTests::rejectsMalformedColors()
{
    QVERIFY(!isValidContainerColor(QStringLiteral("0091FF")));
    QVERIFY(!isValidContainerColor(QStringLiteral("#0091F")));
    QVERIFY(!isValidContainerColor(QStringLiteral("#0091FFF")));
    QVERIFY(!isValidContainerColor(QStringLiteral("#GGGGGG")));
    QVERIFY(!isValidContainerColor(QString{}));
    QVERIFY(normalizedContainerColor(QStringLiteral("not-a-color")).isEmpty());
}

void ContainerAppearanceTests::swatchesAreUniqueValidNonDefaultColors()
{
    const auto swatches = containerColorSwatches();
    QVERIFY(!swatches.isEmpty());
    QSet<QString> seen;
    for (const auto &swatch : swatches) {
        QVERIFY(isValidContainerColor(swatch.colorHex));
        QVERIFY(!swatch.label.isEmpty());
        QVERIFY(!seen.contains(swatch.colorHex));
        seen.insert(swatch.colorHex);
    }
}

QTEST_GUILESS_MAIN(ContainerAppearanceTests)
#include "tst_containerappearance.moc"
