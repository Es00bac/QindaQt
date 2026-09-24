// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpreferencevalues.h"

#include "default_layout_profile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace QindaQt::Shell;

namespace {

QVariantMap validSnapshotValues()
{
    return {{QStringLiteral("panels.layoutProfile"), QStringLiteral("xfce-inspired")},
            {QStringLiteral("appearance.theme"), QStringLiteral("qinda-light")},
            {QStringLiteral("appearance.colorScheme"), QStringLiteral("light")},
            {QStringLiteral("appearance.wallpaper"), QStringLiteral("qindaqt:jade-fold")},
            {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
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
    void defaultLayoutMatchesSettingsDefaults();
    void resolvesBundledAndCustomWallpapers();
};

void ShellPreferenceValuesTests::decodesCompleteSnapshot()
{
    QString error;
    const auto values =
        ShellPreferenceValues::fromVariantMap(validSnapshotValues(), &error);
    QVERIFY2(values.has_value(), qPrintable(error));
    QCOMPARE(values->layoutProfileId, QStringLiteral("xfce-inspired"));
    QCOMPARE(values->themeId, QStringLiteral("qinda-light"));
    QCOMPARE(values->colorScheme, QStringLiteral("light"));
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
    QCOMPARE(keys.size(), 11);
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
             QStringLiteral("xfce-inspired"));
    // ADR-0263: with no confirmed preference the Mac-style layout is used.
    QCOMPARE(resolveStartupProfileId({}, std::nullopt),
             QStringLiteral("macos-inspired"));

    QCOMPARE(resolveStartupThemeId(QStringLiteral("qinda-dark"), preferences,
                                   QStringLiteral("profile-default")),
             QStringLiteral("qinda-dark"));
    QCOMPARE(resolveStartupThemeId({}, preferences, QStringLiteral("profile-default")),
             QStringLiteral("qinda-light"));
    QCOMPARE(resolveStartupThemeId({}, std::nullopt, QStringLiteral("profile-default")),
             QStringLiteral("profile-default"));
}


namespace {

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

// ADR-0263: the shell fallback, the Settings1 schema default, and the
// distribution profile-defaults layer must all name the same installed
// stock layout, or a new user and a degraded startup would disagree.
void ShellPreferenceValuesTests::defaultLayoutMatchesSettingsDefaults()
{
    const QString expected = QString::fromLatin1(DefaultLayoutProfileId);
    QCOMPARE(expected, QStringLiteral("macos-inspired"));

    const QJsonObject schema = readJsonObject(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"));
    QString schemaDefault;
    for (const QJsonValue &entry : schema.value(QStringLiteral("settings")).toArray()) {
        const QJsonObject key = entry.toObject();
        if (key.value(QStringLiteral("key")).toString()
            == QStringLiteral("panels.layoutProfile")) {
            schemaDefault = key.value(QStringLiteral("default")).toString();
        }
    }
    QCOMPARE(schemaDefault, expected);

    const QJsonObject profileDefaults = readJsonObject(QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"));
    QCOMPARE(profileDefaults.value(QStringLiteral("values")).toObject()
                 .value(QStringLiteral("panels.layoutProfile")).toString(),
             expected);

    const QJsonObject stock = readJsonObject(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/") + expected
        + QStringLiteral(".json"));
    QCOMPARE(stock.value(QStringLiteral("id")).toString(), expected);
}

void ShellPreferenceValuesTests::resolvesBundledAndCustomWallpapers()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("qindaqt/wallpapers"))));
    const auto plant = [&](const QString &name) {
        const QString path = root.filePath(QStringLiteral("qindaqt/wallpapers/") + name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return QString{};
        }
        file.write("image");
        file.close();
        return path;
    };
    const QString bundled = plant(QStringLiteral("jade-fold.png"));
    // Chooser formats beyond PNG resolve for the shell too; the catalog and
    // the shell must agree or a bundled pick would preview but never paint.
    const QString jpgOnly = plant(QStringLiteral("photo-dune.jpg"));
    const QString webpOnly = plant(QStringLiteral("paper-moon.webp"));
    const QString duoPng = plant(QStringLiteral("duo.png"));
    plant(QStringLiteral("duo.jpg"));
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:jade-fold"), {root.path()}), bundled);
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:photo-dune"), {root.path()}), jpgOnly);
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:paper-moon"), {root.path()}), webpOnly);
    // One identity, two formats on disk: the priority format wins so both
    // ends of the contract name the same file.
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:duo"), {root.path()}), duoPng);
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:../escape"), {root.path()}), QString{});
    QCOMPARE(resolveWallpaperSource(QStringLiteral("qindaqt:missing"), {root.path()}), QString{});
    QCOMPARE(resolveWallpaperSource(QString{}, {root.path()}), QString{});
    QCOMPARE(resolveWallpaperSource(bundled, {}), bundled);
    QCOMPARE(resolveWallpaperSource(QStringLiteral("relative.png"), {}), QString{});
}

QTEST_GUILESS_MAIN(ShellPreferenceValuesTests)
#include "tst_shellpreferencevalues.moc"
