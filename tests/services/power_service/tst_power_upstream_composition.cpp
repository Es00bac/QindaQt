// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_session_collaborator.h>
#include <qindaqt/services/power_service/adapters/power_profiles_collaborator.h>
#include <qindaqt/services/power_service/adapters/production_battery_collaborator.h>
#include <qindaqt/services/power_service/adapters/upstream_composition.h>
#include <qindaqt/services/power_service/unavailable_power_collaborators.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

using namespace QindaQt::Power;

class PowerUpstreamCompositionTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesExactModeTokens();
    void rejectsUnknownModes();
    void unavailableModeComposesDeterministicCollaborators();
    void productionModeComposesAdaptersOnInjectedBus();
    void productionModeSurvivesDisconnectedBus();
};

void PowerUpstreamCompositionTests::parsesExactModeTokens()
{
    QCOMPARE(Upstream::parseUpstreamMode(QStringLiteral("unavailable")),
             std::optional(Upstream::UpstreamMode::Unavailable));
    QCOMPARE(Upstream::parseUpstreamMode(QStringLiteral("production")),
             std::optional(Upstream::UpstreamMode::Production));
    // Only the exact tokens select a mode; there are no aliases.
    QVERIFY(!Upstream::parseUpstreamMode(QStringLiteral("prod")).has_value());
    QVERIFY(!Upstream::parseUpstreamMode(QStringLiteral("PRODUCTION")).has_value());
    QVERIFY(!Upstream::parseUpstreamMode(QString()).has_value());
}

void PowerUpstreamCompositionTests::rejectsUnknownModes()
{
    QVERIFY(!Upstream::parseUpstreamMode(QStringLiteral("host")).has_value());
    QVERIFY(!Upstream::parseUpstreamMode(QStringLiteral("upower")).has_value());
}

void PowerUpstreamCompositionTests::unavailableModeComposesDeterministicCollaborators()
{
    // An invalid placeholder connection proves the unavailable mode never
    // opens an upstream bus.
    const QDBusConnection unused(QStringLiteral("composition-test-unused"));
    Upstream::UpstreamComposition composition = Upstream::composeUpstream(
        Upstream::UpstreamMode::Unavailable, unused, QStringLiteral("/nonexistent"));
    QVERIFY(qobject_cast<UnavailableBatteryCollaborator *>(composition.battery.get())
            != nullptr);
    QVERIFY(qobject_cast<UnavailableProfileCollaborator *>(composition.profiles.get())
            != nullptr);
    QVERIFY(qobject_cast<UnavailableSessionCollaborator *>(composition.session.get())
            != nullptr);
}

void PowerUpstreamCompositionTests::productionModeComposesAdaptersOnInjectedBus()
{
    const QDBusConnection unused(QStringLiteral("composition-test-unused-2"));
    Upstream::UpstreamComposition composition = Upstream::composeUpstream(
        Upstream::UpstreamMode::Production, unused,
        QStringLiteral("/nonexistent-backlight-root"));
    QVERIFY(qobject_cast<Upstream::ProductionBatteryCollaborator *>(
                composition.battery.get())
            != nullptr);
    QVERIFY(qobject_cast<Upstream::PowerProfilesCollaborator *>(
                composition.profiles.get())
            != nullptr);
    QVERIFY(qobject_cast<Upstream::LogindSessionCollaborator *>(
                composition.session.get())
            != nullptr);
}

void PowerUpstreamCompositionTests::productionModeSurvivesDisconnectedBus()
{
    const QDBusConnection unused(QStringLiteral("composition-test-unused-3"));
    Upstream::UpstreamComposition composition = Upstream::composeUpstream(
        Upstream::UpstreamMode::Production, unused, QString());
    // Starting every production collaborator on a disconnected bus must not
    // crash; each reports its fail-closed unavailability instead.
    const quint64 batteryGeneration = composition.battery->start();
    const quint64 profileGeneration = composition.profiles->start();
    const quint64 sessionGeneration = composition.session->start();
    QVERIFY(batteryGeneration != 0);
    QVERIFY(profileGeneration != 0);
    QVERIFY(sessionGeneration != 0);
    composition.battery->stop();
    composition.profiles->stop();
    composition.session->stop();
}

QTEST_GUILESS_MAIN(PowerUpstreamCompositionTests)
#include "tst_power_upstream_composition.moc"
