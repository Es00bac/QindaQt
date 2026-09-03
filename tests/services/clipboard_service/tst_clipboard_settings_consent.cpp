// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard_history_consent.h"

#include <QtTest/QTest>

using namespace QindaQt::Services;

namespace {

SettingsClient::SettingsSnapshot resolved(bool enabled, const QString &sourceLayer)
{
    return {.owner = QStringLiteral(":fixture"),
            .epoch = QStringLiteral("fixture-epoch"),
            .settingsSchemaVersion = 2,
            .revision = 1,
            .values = {{QStringLiteral("services.clipboardHistory"), enabled}},
            .sourceLayers = {{QStringLiteral("services.clipboardHistory"), sourceLayer}}};
}

} // namespace

class ClipboardSettingsConsentTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void schemaDefaultTrueWithoutOverrideIsDenied()
    {
        const auto legacyDefault = resolved(true, QStringLiteral("system-defaults"));
        QVERIFY(!Clipboard::hasExplicitHistoryConsent(legacyDefault));
        QVERIFY(!Clipboard::hasExplicitHistoryConsent(
            resolved(true, QStringLiteral("profile-defaults"))));
        QVERIFY(!Clipboard::hasExplicitHistoryConsent(std::nullopt));
    }

    void userOverrideTrueIsAccepted()
    {
        QVERIFY(Clipboard::hasExplicitHistoryConsent(
            resolved(true, QStringLiteral("user-overrides"))));
        QVERIFY(!Clipboard::hasExplicitHistoryConsent(
            resolved(false, QStringLiteral("user-overrides"))));

        auto wrongType = resolved(true, QStringLiteral("user-overrides"));
        wrongType.values.insert(QStringLiteral("services.clipboardHistory"),
                                QStringLiteral("true"));
        QVERIFY(!Clipboard::hasExplicitHistoryConsent(wrongType));
    }
};

QTEST_GUILESS_MAIN(ClipboardSettingsConsentTest)
#include "tst_clipboard_settings_consent.moc"
