// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/window_management_preferences.h"

#include <QTest>

using namespace QindaQt::Session::WindowManagement;

namespace {

QVariantMap defaults()
{
    return {{QStringLiteral("windowManagement.focusPolicy"), QStringLiteral("click")},
            {QStringLiteral("windowManagement.dockingModifier"), QStringLiteral("super")},
            {QStringLiteral("windowManagement.snapDistance"), 12},
            {QStringLiteral("windowManagement.sessionRestore"), true},
            {QStringLiteral("windowManagement.closeContainerPolicy"), QStringLiteral("ask")}};
}

} // namespace

class WindowManagementPreferencesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scopedKeysAreTheFiveSchemaKeys();
    void schemaDefaultsDecodeToTheStructDefaults();
    void everyEnumSpellingDecodes();
    void aMissingOrInvalidEntryRejectsTheWholeSnapshot();
    void kwinSpellingsAreStable();
};

void WindowManagementPreferencesTest::scopedKeysAreTheFiveSchemaKeys()
{
    const QStringList keys = WindowManagementPreferences::scopedKeys();
    QCOMPARE(keys.size(), 5);
    for (const QString &key : keys) {
        QVERIFY2(key.startsWith(QLatin1String("windowManagement.")), qPrintable(key));
        QVERIFY(defaults().contains(key));
    }
}

void WindowManagementPreferencesTest::schemaDefaultsDecodeToTheStructDefaults()
{
    QString error;
    const auto decoded = WindowManagementPreferences::fromVariantMap(defaults(), &error);
    QVERIFY2(decoded.has_value(), qPrintable(error));
    QVERIFY(*decoded == WindowManagementPreferences{});
    QVERIFY(error.isEmpty());
}

void WindowManagementPreferencesTest::everyEnumSpellingDecodes()
{
    QVariantMap values = defaults();
    values[QStringLiteral("windowManagement.focusPolicy")] = QStringLiteral("focus-follows-mouse");
    values[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("alt");
    values[QStringLiteral("windowManagement.snapDistance")] = 0;
    values[QStringLiteral("windowManagement.sessionRestore")] = false;
    values[QStringLiteral("windowManagement.closeContainerPolicy")] = QStringLiteral("close-all");
    auto decoded = WindowManagementPreferences::fromVariantMap(values);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->focusPolicy == FocusPolicy::FocusFollowsMouse);
    QVERIFY(decoded->dockingModifier == DockingModifier::Alt);
    QCOMPARE(decoded->snapDistance, 0);
    QVERIFY(!decoded->sessionRestore);
    QVERIFY(decoded->closeContainerPolicy == CloseContainerPolicy::CloseAll);

    values[QStringLiteral("windowManagement.focusPolicy")] = QStringLiteral("focus-under-mouse");
    values[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("control");
    // JSON-sourced integers arrive as doubles over the wire.
    values[QStringLiteral("windowManagement.snapDistance")] = 64.0;
    values[QStringLiteral("windowManagement.closeContainerPolicy")] = QStringLiteral("ungroup");
    decoded = WindowManagementPreferences::fromVariantMap(values);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->focusPolicy == FocusPolicy::FocusUnderMouse);
    QVERIFY(decoded->dockingModifier == DockingModifier::Control);
    QCOMPARE(decoded->snapDistance, 64);
    QVERIFY(decoded->closeContainerPolicy == CloseContainerPolicy::Ungroup);

    values[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("disabled");
    decoded = WindowManagementPreferences::fromVariantMap(values);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->dockingModifier == DockingModifier::Disabled);
}

void WindowManagementPreferencesTest::aMissingOrInvalidEntryRejectsTheWholeSnapshot()
{
    QString error;
    QVariantMap values = defaults();
    values.remove(QStringLiteral("windowManagement.sessionRestore"));
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
    QVERIFY2(error.contains(QLatin1String("sessionRestore")), qPrintable(error));

    values = defaults();
    values[QStringLiteral("windowManagement.focusPolicy")] = QStringLiteral("sloppy");
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
    QVERIFY2(error.contains(QLatin1String("sloppy")), qPrintable(error));

    values = defaults();
    values[QStringLiteral("windowManagement.snapDistance")] = 65;
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
    values[QStringLiteral("windowManagement.snapDistance")] = 2.5;
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
    values[QStringLiteral("windowManagement.snapDistance")] = QStringLiteral("12");
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());

    values = defaults();
    values[QStringLiteral("windowManagement.closeContainerPolicy")] = QStringLiteral("nuke");
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
    values = defaults();
    values[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("hyper");
    QVERIFY(!WindowManagementPreferences::fromVariantMap(values, &error).has_value());
}

void WindowManagementPreferencesTest::kwinSpellingsAreStable()
{
    QCOMPARE(WindowManagementPreferences::kwinFocusPolicy(FocusPolicy::Click),
             QStringLiteral("ClickToFocus"));
    QCOMPARE(WindowManagementPreferences::kwinFocusPolicy(FocusPolicy::FocusFollowsMouse),
             QStringLiteral("FocusFollowsMouse"));
    QCOMPARE(WindowManagementPreferences::kwinFocusPolicy(FocusPolicy::FocusUnderMouse),
             QStringLiteral("FocusUnderMouse"));
    QCOMPARE(WindowManagementPreferences::dockingModifierName(DockingModifier::Disabled),
             QStringLiteral("disabled"));
    QCOMPARE(WindowManagementPreferences::closeContainerPolicyName(CloseContainerPolicy::CloseAll),
             QStringLiteral("close-all"));
}

QTEST_GUILESS_MAIN(WindowManagementPreferencesTest)
#include "tst_window_management_preferences.moc"
