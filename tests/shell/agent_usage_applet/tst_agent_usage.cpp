// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture_source.h"
#include "agent_usage_applet_controller.h"
#include "agentusageappletcomposition.h"
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/applet_host/capability_policy.h>
#include <qindaqt/applet_host/capability_policy_loader.h>
#include <QtTest>
using namespace QindaQt;
using namespace QindaQt::Shell::AgentUsageApplet;
class AgentUsageTests final : public QObject {
    Q_OBJECT
private slots:
    void projectionPreservesUnknownAndIndependentScope() {
        const auto rows = providerRows({fixtureProvider()});
        const auto row = rows.front().toMap();
        QVERIFY(row.value("metricsAvailable").toBool());
        QVERIFY(row.value("tokens").toString().contains("Total tokens: 0"));
        QVERIFY(row.value("tokens").toString().contains("input: Not reported"));
        QCOMPARE(row.value("tokenScope").toString(), QStringLiteral("Tokens: context"));
        QCOMPARE(row.value("costScope").toString(), QStringLiteral("Cost: session"));
        QVERIFY(row.value("cost").toString().contains("1.2500"));
        QCOMPARE(row.value("state").toString(), QStringLiteral("Stale"));
        QVERIFY(row.value("quotas").toStringList().front().contains("75.0% remaining"));
        QVERIFY(row.value("quotas").toStringList().front().contains("resets"));
        auto unknown = fixtureProvider();
        unknown.totalTokens.reset(); unknown.reportedCostUsd.reset();
        unknown.quotaWindows = {{QStringLiteral("weekly"), {}, {}}};
        const auto missing = providerRows({unknown}).front().toMap();
        QVERIFY(!missing.value("metricsAvailable").toBool());
        QVERIFY(missing.value("tokens").toString().contains("Total tokens: Not reported"));
        QVERIFY(missing.value("cost").toString().contains("not reported"));
        QVERIFY(missing.value("quotas").toStringList().front().contains("reset not reported"));
    }
    void overageClampsRemainingAndHostileMetadataStaysLiteral() {
        auto row=fixtureProvider();
        row.quotaWindows.front().usedPercent=125.0;
        row.displayName=QStringLiteral("<img src=\"file:///private/report\"/>");
        row.source=QStringLiteral("<b>publisher</b>");
        const auto projected=providerRows({row}).front().toMap();
        QVERIFY(projected.value("quotas").toStringList().front().contains("0.0% remaining"));
        QCOMPARE(projected.value("name").toString(),row.displayName);
        QCOMPARE(projected.value("source").toString(),row.source);
    }
    void denialNeverReadsOrRefreshes() {
        FixtureSource source; source.values = {fixtureProvider()};
        AgentUsageAppletController controller(&source, false);
        controller.refresh(); controller.checkFreshness();
        QCOMPARE(source.snapshots, 0); QCOMPARE(source.refreshes, 0);
        QVERIFY(controller.rows().isEmpty()); QVERIFY(controller.diagnostic().contains("denied"));
    }
    void freshnessProjectionDoesNotCollect() {
        FixtureSource source; auto provider=fixtureProvider();
        provider.state=UsageState::Ready; source.values={provider}; source.projectFreshness=true;
        AgentUsageAppletController controller(&source,true);
        QCOMPARE(controller.rows().front().toMap().value("state").toString(),QStringLiteral("Current"));
        source.clock=source.clock.addSecs(16*60); controller.checkFreshness();
        QCOMPARE(controller.rows().front().toMap().value("state").toString(),QStringLiteral("Stale"));
        QCOMPARE(source.refreshes,0);
    }
    void publicationAndSourceLifetime() {
        auto source = std::make_unique<FixtureSource>();
        source->values = {fixtureProvider()};
        AgentUsageAppletController controller(source.get(), true);
        QCOMPARE(controller.rows().size(), 1);
        source->values.front().displayName = QStringLiteral("Updated");
        controller.refresh();
        QCOMPARE(source->refreshes, 1);
        QCOMPARE(controller.rows().front().toMap().value("name").toString(), QStringLiteral("Updated"));
        source.reset();
        QVERIFY(controller.rows().isEmpty()); controller.refresh();
        QVERIFY(controller.diagnostic().contains("unavailable"));
    }
    void auditedCompositionDefersReadsAndDenialSkipsFactory() {
        Applets::ManifestCatalog catalog; QString error;
        QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error), qPrintable(error));
        AppletHost::CapabilityPolicy policy;
        int constructions = 0;
        FixtureSource *created = nullptr;
        auto factory = [&]() -> std::unique_ptr<AgentUsageSource> {
            ++constructions;
            auto source = std::make_unique<FixtureSource>();
            source->values = {fixtureProvider()};
            created = source.get();
            return source;
        };
        const auto *manifest = catalog.findById(QStringLiteral("agent-usage"));
        QVERIFY(manifest);
        const auto shipped = AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
        QVERIFY2(shipped.ok, qPrintable(shipped.error));
        const auto thirdParty = shipped.policy.evaluate(*manifest,
            {manifest->id, AppletHost::PackageTrust::ThirdParty});
        QVERIFY(thirdParty.ok);
        QCOMPARE(thirdParty.decisions.size(), 1);
        QVERIFY(!thirdParty.decisions.front().granted());
        Shell::AgentUsageAppletComposition denied(catalog, policy, factory);
        denied.access()->refresh(); QCOMPARE(constructions, 0);
        policy.auditedBuiltinDefault = AppletHost::CapabilityDisposition::Grant;
        Shell::AgentUsageAppletComposition allowed(catalog, policy, factory);
        QCOMPARE(constructions, 1);
        QVERIFY(created); QCOMPARE(created->refreshes, 0);
        QVERIFY(allowed.access()->readGranted());
        QCOMPARE(allowed.access()->rows().size(), 1);
    }
};
QTEST_GUILESS_MAIN(AgentUsageTests)
#include "tst_agent_usage.moc"
