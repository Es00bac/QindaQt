// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bootstrap.h"

#include <QGuiApplication>
#include <QtTest>

using namespace QindaQt::Services::FontPreferences;

class FontSettingsBootstrapTests final : public QObject {
    Q_OBJECT
private slots:
    void applyPreferencesChangesDefaultFont();
    void applyPreferencesRejectsInvalidPreferences();
    void confirmedFamilyResolvesCaseInsensitively();
    void confirmedFamilyNeverResolvesInEmptyDiscovery();
};

void FontSettingsBootstrapTests::applyPreferencesChangesDefaultFont()
{
    const QFont before = QGuiApplication::font();
    FontPreferences preferences;
    preferences.setFamily(QStringLiteral("QindaQt Bootstrap Probe"));
    preferences.setPointSize(14.0);
    preferences.setHinting(FontHinting::None);
    preferences.setAntialiasing(FontAntialiasing::None);

    QVERIFY(FontSettingsBootstrap::applyPreferences(preferences));
    const QFont after = QGuiApplication::font();
    QCOMPARE(after.family(), QStringLiteral("QindaQt Bootstrap Probe"));
    QCOMPARE(after.pointSizeF(), 14.0);
    QCOMPARE(after.hintingPreference(), QFont::PreferNoHinting);
    QVERIFY(after.styleStrategy() & QFont::NoAntialias);
    QVERIFY(after != before);
}

void FontSettingsBootstrapTests::applyPreferencesRejectsInvalidPreferences()
{
    const QFont before = QGuiApplication::font();
    FontPreferences preferences;
    preferences.setFamily(QString(QChar(0x01)));
    QVERIFY(!preferences.isValid());

    QString diagnostic;
    QVERIFY(!FontSettingsBootstrap::applyPreferences(preferences, &diagnostic));
    QVERIFY(!diagnostic.isEmpty());
    QCOMPARE(QGuiApplication::font(), before);
}

void FontSettingsBootstrapTests::confirmedFamilyResolvesCaseInsensitively()
{
    FontPreferences preferences;
    preferences.setFamily(QStringLiteral("liberation mono"));

    FontFact fact;
    fact.family = QStringLiteral("Liberation Mono");
    QVERIFY(FontSettingsBootstrap::confirmedFamilyResolves(preferences, {fact}));

    fact.family = QStringLiteral("Liberation Serif");
    QVERIFY(!FontSettingsBootstrap::confirmedFamilyResolves(preferences, {fact}));

    preferences.setFamily(QStringLiteral("  Liberation Mono  "));
    fact.family = QStringLiteral("Liberation Mono");
    QVERIFY(FontSettingsBootstrap::confirmedFamilyResolves(preferences, {fact}));
}

void FontSettingsBootstrapTests::confirmedFamilyNeverResolvesInEmptyDiscovery()
{
    FontPreferences preferences;
    preferences.setFamily(QStringLiteral("Noto Sans"));
    QVERIFY(!FontSettingsBootstrap::confirmedFamilyResolves(preferences, {}));
}

QTEST_MAIN(FontSettingsBootstrapTests)
#include "tst_font_settings_bootstrap.moc"
