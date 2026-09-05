// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpreferencevalues.h"

#include <QtTest>

using namespace QindaQt::Shell;

namespace {

QVariantMap validSnapshotValues()
{
    return {{QStringLiteral("panels.layoutProfile"), QStringLiteral("mate-inspired")},
            {QStringLiteral("appearance.theme"), QStringLiteral("qinda-light")},
            {QStringLiteral("fonts.family"), QStringLiteral("Noto Serif")},
            {QStringLiteral("fonts.pointSize"), 11.5},
            {QStringLiteral("accessibility.highContrast"), true},
            {QStringLiteral("accessibility.reducedMotion"), false},
            {QStringLiteral("accessibility.reducedTransparency"), true},
            {QStringLiteral("accessibility.textScale"), 1.25}};
}

} // namespace

class ShellPreferenceValuesTests final : public QObject {
    Q_OBJECT

private slots:
    void decodesCompleteSnapshot();
    void acceptsIntegralWireNumbers();
    void clampsIntoAccessibilityBounds();
    void rejectsMissingKey();
    void rejectsMistypedValue();
    void rejectsBlankStrings();
    void scopedKeysCoverEveryDecodedKey();
    void startupSelectionPrecedence();
};

void ShellPreferenceValuesTests::decodesCompleteSnapshot()
{
    QString error;
    const auto values =
        ShellPreferenceValues::fromVariantMap(validSnapshotValues(), &error);
    QVERIFY2(values.has_value(), qPrintable(error));
    QCOMPARE(values->layoutProfileId, QStringLiteral("mate-inspired"));
    QCOMPARE(values->themeId, QStringLiteral("qinda-light"));
    QCOMPARE(values->fontFamily, QStringLiteral("Noto Serif"));
    QCOMPARE(values->accessibility.basePointSize, 11.5);
    QCOMPARE(values->accessibility.textScale, 1.25);
    QVERIFY(values->accessibility.highContrast);
    QVERIFY(!values->accessibility.reducedMotion);
    QVERIFY(values->accessibility.reducedTransparency);
}

void ShellPreferenceValuesTests::acceptsIntegralWireNumbers()
{
    QVariantMap snapshot = validSnapshotValues();
    snapshot.insert(QStringLiteral("fonts.pointSize"), QVariant::fromValue(qint64(12)));
    snapshot.insert(QStringLiteral("accessibility.textScale"), QVariant::fromValue(qint64(2)));
    QString error;
    const auto values = ShellPreferenceValues::fromVariantMap(snapshot, &error);
    QVERIFY2(values.has_value(), qPrintable(error));
    QCOMPARE(values->accessibility.basePointSize, 12.0);
    QCOMPARE(values->accessibility.textScale, 2.0);
}

void ShellPreferenceValuesTests::clampsIntoAccessibilityBounds()
{
    QVariantMap snapshot = validSnapshotValues();
    snapshot.insert(QStringLiteral("accessibility.textScale"), 99.0);
    const auto values = ShellPreferenceValues::fromVariantMap(snapshot);
    QVERIFY(values.has_value());
    QCOMPARE(values->accessibility.textScale,
             QindaQt::DesignTokens::AccessibilityInputs::maximumTextScale);
}

void ShellPreferenceValuesTests::rejectsMissingKey()
{
    QVariantMap snapshot = validSnapshotValues();
    snapshot.remove(QStringLiteral("appearance.theme"));
    QString error;
    QVERIFY(!ShellPreferenceValues::fromVariantMap(snapshot, &error).has_value());
    QVERIFY(!error.isEmpty());

    QVariantMap noFamily = validSnapshotValues();
    noFamily.remove(QStringLiteral("fonts.family"));
    QVERIFY(!ShellPreferenceValues::fromVariantMap(noFamily).has_value());
}

void ShellPreferenceValuesTests::rejectsMistypedValue()
{
    QVariantMap snapshot = validSnapshotValues();
    snapshot.insert(QStringLiteral("accessibility.highContrast"), QStringLiteral("yes"));
    QVERIFY(!ShellPreferenceValues::fromVariantMap(snapshot).has_value());
}

void ShellPreferenceValuesTests::rejectsBlankStrings()
{
    QVariantMap snapshot = validSnapshotValues();
    snapshot.insert(QStringLiteral("panels.layoutProfile"), QStringLiteral("  "));
    QVERIFY(!ShellPreferenceValues::fromVariantMap(snapshot).has_value());
}

void ShellPreferenceValuesTests::scopedKeysCoverEveryDecodedKey()
{
    const QStringList keys = ShellPreferenceValues::scopedKeys();
    QCOMPARE(keys.size(), 8);
    for (const QString &key : validSnapshotValues().keys()) {
        QVERIFY2(keys.contains(key), qPrintable(key));
    }
}

void ShellPreferenceValuesTests::startupSelectionPrecedence()
{
    const auto preferences =
        ShellPreferenceValues::fromVariantMap(validSnapshotValues());
    QVERIFY(preferences.has_value());

    // Explicit CLI outranks the confirmed preference; the preference outranks
    // the built-in/profile defaults.
    QCOMPARE(resolveStartupProfileId(QStringLiteral("gnome-inspired"), preferences),
             QStringLiteral("gnome-inspired"));
    QCOMPARE(resolveStartupProfileId({}, preferences),
             QStringLiteral("mate-inspired"));
    QCOMPARE(resolveStartupProfileId({}, std::nullopt), QStringLiteral("qindaqt"));

    QCOMPARE(resolveStartupThemeId(QStringLiteral("qinda-dark"), preferences,
                                   QStringLiteral("profile-default")),
             QStringLiteral("qinda-dark"));
    QCOMPARE(resolveStartupThemeId({}, preferences, QStringLiteral("profile-default")),
             QStringLiteral("qinda-light"));
    QCOMPARE(resolveStartupThemeId({}, std::nullopt, QStringLiteral("profile-default")),
             QStringLiteral("profile-default"));
}

QTEST_GUILESS_MAIN(ShellPreferenceValuesTests)
#include "tst_shellpreferencevalues.moc"
