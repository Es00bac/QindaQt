// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences_coordinator.h"

#include <QTest>

using namespace QindaQt::Services::FontPreferences;

class tst_FontPreferencesCoordinator : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitialState();
    void testRefreshCatalogSuccess();
    void testRefreshCatalogFailurePreservesLKG();
    void testUpdatePreferencesSuccess();
    void testUpdatePreferencesFailurePreservesLKG();
    void testUpdateFromSettings();
    void testResetToDefaults();
};

void tst_FontPreferencesCoordinator::testInitialState()
{
    FontPreferencesCoordinator coordinator;
    QCOMPARE(coordinator.revision(), 1);
    QCOMPARE(coordinator.preferences(), FontPreferences::systemDefaults());
    QCOMPARE(coordinator.lastKnownGoodPreferences(), FontPreferences::systemDefaults());
    QVERIFY(!coordinator.catalog().isEmpty());
    QCOMPARE(coordinator.catalog(), coordinator.lastKnownGoodCatalog());
}

void tst_FontPreferencesCoordinator::testRefreshCatalogSuccess()
{
    FontPreferencesCoordinator coordinator;
    const qint64 initialRev = coordinator.revision();

    QList<FontFact> facts = {
        {QStringLiteral("Hack"), QStringLiteral("Regular"), true, true, 400, false, QString()},
        {QStringLiteral("Inter"), QStringLiteral("Regular"), false, true, 400, false, QString()}
    };

    QString error;
    const bool ok = coordinator.refreshCatalog(facts, &error);
    QVERIFY(ok);
    QVERIFY(error.isEmpty());
    QCOMPARE(coordinator.revision(), initialRev + 1);
    QCOMPARE(coordinator.catalog().familyCount(), 2);
    QCOMPARE(coordinator.lastKnownGoodCatalog().familyCount(), 2);
    QVERIFY(coordinator.catalog().containsFamily(QStringLiteral("Hack")));
}

void tst_FontPreferencesCoordinator::testRefreshCatalogFailurePreservesLKG()
{
    FontPreferencesCoordinator coordinator;
    const FontCatalog baselineCatalog = coordinator.catalog();
    const qint64 initialRev = coordinator.revision();

    // Hostile facts: empty or conflicting
    QString error;
    bool ok = coordinator.refreshCatalog({}, &error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
    QCOMPARE(coordinator.revision(), initialRev);
    QCOMPARE(coordinator.catalog(), baselineCatalog);
    QCOMPARE(coordinator.lastKnownGoodCatalog(), baselineCatalog);

    QList<FontFact> conflictingFacts = {
        {QStringLiteral("Monaspace"), QStringLiteral("Regular"), true, true, 400, false, QString()},
        {QStringLiteral("Monaspace"), QStringLiteral("Bold"), false, true, 700, false, QString()}
    };
    ok = coordinator.refreshCatalog(conflictingFacts, &error);
    QVERIFY(!ok);
    QCOMPARE(coordinator.revision(), initialRev);
    QCOMPARE(coordinator.catalog(), baselineCatalog);
}

void tst_FontPreferencesCoordinator::testUpdatePreferencesSuccess()
{
    FontPreferencesCoordinator coordinator;
    const qint64 initialRev = coordinator.revision();

    FontPreferences newPrefs;
    newPrefs.setFamily(QStringLiteral("Cantarell"));
    newPrefs.setPointSize(12.0);

    QString error;
    const bool ok = coordinator.updatePreferences(newPrefs, &error);
    QVERIFY(ok);
    QCOMPARE(coordinator.revision(), initialRev + 1);
    QCOMPARE(coordinator.preferences().family(), QStringLiteral("Cantarell"));
    QCOMPARE(coordinator.lastKnownGoodPreferences().family(), QStringLiteral("Cantarell"));
}

void tst_FontPreferencesCoordinator::testUpdatePreferencesFailurePreservesLKG()
{
    FontPreferencesCoordinator coordinator;
    const FontPreferences baselinePrefs = coordinator.preferences();
    const qint64 initialRev = coordinator.revision();

    FontPreferences invalidPrefs;
    invalidPrefs.setFamily(QStringLiteral("")); // invalid empty family

    QString error;
    const bool ok = coordinator.updatePreferences(invalidPrefs, &error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
    QCOMPARE(coordinator.revision(), initialRev);
    QCOMPARE(coordinator.preferences(), baselinePrefs);
    QCOMPARE(coordinator.lastKnownGoodPreferences(), baselinePrefs);
}

void tst_FontPreferencesCoordinator::testUpdateFromSettings()
{
    FontPreferencesCoordinator coordinator;
    const qint64 initialRev = coordinator.revision();

    QVariantMap settings;
    settings.insert(QStringLiteral("fonts.family"), QStringLiteral("Roboto"));
    settings.insert(QStringLiteral("fonts.pointSize"), 11.0);
    settings.insert(QStringLiteral("fonts.hinting"), QStringLiteral("full"));

    QString error;
    const bool ok = coordinator.updateFromSettings(settings, &error);
    QVERIFY(ok);
    QCOMPARE(coordinator.revision(), initialRev + 1);
    QCOMPARE(coordinator.preferences().family(), QStringLiteral("Roboto"));
    QCOMPARE(coordinator.preferences().pointSize(), 11.0);
    QCOMPARE(coordinator.preferences().hinting(), FontHinting::Full);

    // Hostile settings rejection across 36-144 range, below 6.0, and non-finite floats
    const QList<double> hostileSizes = {
        0.0, 4.0, 5.9, 36.1, 50.0, 100.0, 144.0, 9999.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double badSize : hostileSizes) {
        const qint64 revBefore = coordinator.revision();
        const FontPreferences lkgBefore = coordinator.lastKnownGoodPreferences();

        QVariantMap badSettings;
        badSettings.insert(QStringLiteral("fonts.pointSize"), badSize);
        const bool badOk = coordinator.updateFromSettings(badSettings, &error);

        QVERIFY(!badOk);
        QVERIFY(!error.isEmpty());
        QCOMPARE(coordinator.revision(), revBefore);
        QCOMPARE(coordinator.preferences(), lkgBefore);
        QCOMPARE(coordinator.lastKnownGoodPreferences(), lkgBefore);
    }
}

void tst_FontPreferencesCoordinator::testResetToDefaults()
{
    FontPreferencesCoordinator coordinator;
    FontPreferences custom;
    custom.setFamily(QStringLiteral("CustomFont"));
    coordinator.updatePreferences(custom);

    const qint64 revBeforeReset = coordinator.revision();
    coordinator.resetToDefaults();
    QCOMPARE(coordinator.revision(), revBeforeReset + 1);
    QCOMPARE(coordinator.preferences(), FontPreferences::systemDefaults());
}

QTEST_MAIN(tst_FontPreferencesCoordinator)
#include "tst_font_preferences_coordinator.moc"
