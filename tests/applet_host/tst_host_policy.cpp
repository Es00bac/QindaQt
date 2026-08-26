// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"

#include "qindaqt/applets/manifest_loader.h"

#include <QTest>

using namespace QindaQt::AppletHost;
using namespace QindaQt::Applets;

namespace {

AppletManifest loadManifest(const QString &name)
{
    const ManifestLoadResult result = ManifestLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets/") + name + QStringLiteral(".json"));
    if (!result.ok) {
        qFatal("Cannot load test manifest: %s", qPrintable(result.error));
    }
    return result.manifest;
}

CapabilityPolicy loadPolicy()
{
    const CapabilityPolicyLoadResult result = CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    if (!result.ok) {
        qFatal("Cannot load test policy: %s", qPrintable(result.error));
    }
    return result.policy;
}

const CapabilityDecision *findDecision(const CapabilityEvaluation &evaluation,
                                       Capability capability)
{
    for (const CapabilityDecision &decision : evaluation.decisions) {
        if (decision.capability == capability) {
            return &decision;
        }
    }
    return nullptr;
}

} // namespace

class HostPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void selectsOnlyAuditedCompiledCodeInProcess();
    void requiresSandboxForDynamicCode();
    void rejectsThirdPartyBuiltinLookup();
    void evaluatesEveryRequestedCapability();
    void exactRuleOverridesWildcardRule();
    void rejectsDuplicatePolicySelectors();
};

void HostPolicyTest::selectsOnlyAuditedCompiledCodeInProcess()
{
    const AppletManifest launcher = loadManifest(QStringLiteral("launcher"));
    const HostSelection selection = HostSelector::select(
        launcher, {launcher.id, PackageTrust::AuditedBuiltin});
    QVERIFY(selection.accepted());
    QVERIFY(selection.mode == HostMode::InProcessAuditedBuiltin);
}

void HostPolicyTest::requiresSandboxForDynamicCode()
{
    AppletManifest launcher = loadManifest(QStringLiteral("launcher"));
    launcher.entryPoint = {EntryPointKind::Qml, QStringLiteral("ui/Main.qml")};

    const HostSelection thirdParty = HostSelector::select(
        launcher, {launcher.id, PackageTrust::ThirdParty});
    const HostSelection audited = HostSelector::select(
        launcher, {launcher.id, PackageTrust::AuditedBuiltin});
    QVERIFY(thirdParty.mode == HostMode::SandboxRequiredProcess);
    QVERIFY(audited.mode == HostMode::SandboxRequiredProcess);
}

void HostPolicyTest::rejectsThirdPartyBuiltinLookup()
{
    const AppletManifest launcher = loadManifest(QStringLiteral("launcher"));
    const HostSelection selection = HostSelector::select(
        launcher, {launcher.id, PackageTrust::ThirdParty});
    QVERIFY(!selection.accepted());
    QVERIFY(selection.mode == HostMode::Rejected);
    QVERIFY(selection.reason.contains(QStringLiteral("compiled-in")));
}

void HostPolicyTest::evaluatesEveryRequestedCapability()
{
    const CapabilityPolicy policy = loadPolicy();
    const AppletManifest launcher = loadManifest(QStringLiteral("launcher"));
    const CapabilityEvaluation launcherDecision = policy.evaluate(
        launcher, {launcher.id, PackageTrust::ThirdParty});
    QVERIFY2(launcherDecision.ok, qPrintable(launcherDecision.error));
    QCOMPARE(launcherDecision.decisions.size(), launcher.capabilities.size());
    QVERIFY(launcherDecision.decisions.constFirst().granted());
    QVERIFY(launcherDecision.decisions.constFirst().basis == DecisionBasis::SpecificRule);

    const AppletManifest taskList = loadManifest(QStringLiteral("task-list"));
    const CapabilityEvaluation thirdParty = policy.evaluate(
        taskList, {taskList.id, PackageTrust::ThirdParty});
    QVERIFY2(thirdParty.ok, qPrintable(thirdParty.error));
    QCOMPARE(thirdParty.decisions.size(), taskList.capabilities.size());
    for (const CapabilityDecision &decision : thirdParty.decisions) {
        QVERIFY(!decision.granted());
        QVERIFY(!decision.reason.isEmpty());
    }
    const CapabilityDecision *manage = findDecision(thirdParty, Capability::WindowManage);
    QVERIFY(manage != nullptr);
    QVERIFY(manage->basis == DecisionBasis::SpecificRule);

    const CapabilityEvaluation audited = policy.evaluate(
        taskList, {taskList.id, PackageTrust::AuditedBuiltin});
    QVERIFY2(audited.ok, qPrintable(audited.error));
    for (const CapabilityDecision &decision : audited.decisions) {
        QVERIFY(decision.granted());
        QVERIFY(decision.basis == DecisionBasis::TrustDefault);
    }
}

void HostPolicyTest::exactRuleOverridesWildcardRule()
{
    CapabilityPolicy policy = loadPolicy();
    policy.rules.append({.trust = PackageTrust::ThirdParty,
                         .packageId = QStringLiteral("launcher"),
                         .capability = Capability::ApplicationLaunch,
                         .disposition = CapabilityDisposition::Deny,
                         .reason = QStringLiteral("Fixture-specific denial")});
    const AppletManifest launcher = loadManifest(QStringLiteral("launcher"));
    const CapabilityEvaluation evaluation = policy.evaluate(
        launcher, {launcher.id, PackageTrust::ThirdParty});
    QVERIFY2(evaluation.ok, qPrintable(evaluation.error));
    QVERIFY(!evaluation.decisions.constFirst().granted());
    QCOMPARE(evaluation.decisions.constFirst().reason,
             QStringLiteral("Fixture-specific denial"));
}

void HostPolicyTest::rejectsDuplicatePolicySelectors()
{
    CapabilityPolicy policy = loadPolicy();
    policy.rules.append(policy.rules.constFirst());
    const PolicyValidation validation = policy.validate();
    QVERIFY(!validation.isValid());
    QVERIFY(validation.summary().contains(QStringLiteral("duplicate")));
}

QTEST_GUILESS_MAIN(HostPolicyTest)
#include "tst_host_policy.moc"
