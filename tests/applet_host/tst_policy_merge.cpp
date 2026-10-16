// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::AppletHost;
using QindaQt::Applets::Capability;

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

// A one-rule-per-argument policy document. Each rule is
// {trust, packageId, capability, decision}.
struct RuleSpec {
    const char *trust;
    const char *packageId;
    const char *capability;
    const char *decision;
};

QByteArray policyDocument(const char *builtinDefault, const char *thirdPartyDefault,
                          std::initializer_list<RuleSpec> rules)
{
    QByteArray rulesJson;
    for (const RuleSpec &rule : rules) {
        if (!rulesJson.isEmpty()) {
            rulesJson += ",";
        }
        rulesJson += QByteArray(R"({"trust":")") + rule.trust + R"(","packageId":")"
            + rule.packageId + R"(","capability":")" + rule.capability + R"(","decision":")"
            + rule.decision + R"(","reason":"test"})";
    }
    return QByteArray(R"({"schemaVersion":1,"defaults":{"auditedBuiltin":")") + builtinDefault
        + R"(","thirdParty":")" + thirdPartyDefault + R"("},"rules":[)" + rulesJson + "]}";
}

const CapabilityRule *ruleFor(const CapabilityPolicy &policy, PackageTrust trust,
                              const QString &packageId, Capability capability)
{
    for (const CapabilityRule &rule : policy.rules) {
        if (rule.trust == trust && rule.packageId == packageId && rule.capability == capability) {
            return &rule;
        }
    }
    return nullptr;
}

} // namespace

// AGENT-CONTRACT (ADR-0393): the applet policy merges every XDG data
// directory. A higher layer overrides by selector and never deletes, so a
// stale user file cannot shed an installed deny, and an unreadable user file
// cannot take the installed policy down.
class PolicyMergeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void aHigherLayerOverridesBySelectorAndAppendsNewSelectors();
    void aRuleTheHigherLayerDoesNotRestateStillApplies();
    void theHighestLayerSuppliesTheDefaults();
    void anUnreadableOrInvalidOverrideIsSkippedAndReported();
    void anInvalidBaseFailsTheLoad();
    void noFileIsAnError();
    void aSingleFileMatchesFromFile();

private:
    [[nodiscard]] QString file(const QString &name) const { return m_root.filePath(name); }

    QTemporaryDir m_root;
};

void PolicyMergeTest::init()
{
    QVERIFY(m_root.isValid());
    QDir(m_root.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_root.path()));
}

void PolicyMergeTest::aHigherLayerOverridesBySelectorAndAppendsNewSelectors()
{
    QVERIFY(writeFile(file(QStringLiteral("system.json")),
                      policyDocument("grant", "deny",
                                     {{"third-party", "*", "applications.launch", "deny"},
                                      {"audited-builtin", "global-menu", "windows.activate",
                                       "deny"}})));
    QVERIFY(writeFile(file(QStringLiteral("user.json")),
                      policyDocument("grant", "deny",
                                     {{"third-party", "*", "applications.launch", "grant"},
                                      {"third-party", "*", "settings.read", "grant"}})));
    QStringList skipped;
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles(
        {file(QStringLiteral("system.json")), file(QStringLiteral("user.json"))}, &skipped);
    QVERIFY2(merged.ok, qPrintable(merged.error));
    QVERIFY(skipped.isEmpty());
    QCOMPARE(merged.policy.rules.size(), 3);
    // Overridden in place, not duplicated.
    const CapabilityRule *launch = ruleFor(merged.policy, PackageTrust::ThirdParty, QString(),
                                           Capability::ApplicationLaunch);
    QVERIFY(launch != nullptr);
    QVERIFY(launch->disposition == CapabilityDisposition::Grant);
    // Appended.
    const CapabilityRule *settings = ruleFor(merged.policy, PackageTrust::ThirdParty, QString(),
                                             Capability::SettingsRead);
    QVERIFY(settings != nullptr);
    QVERIFY(settings->disposition == CapabilityDisposition::Grant);
    QVERIFY(merged.policy.validate().isValid());
}

void PolicyMergeTest::aRuleTheHigherLayerDoesNotRestateStillApplies()
{
    // The installed policy denies windows.activate to the global menu. A stale
    // user file written before that rule existed must not shed it.
    QVERIFY(writeFile(file(QStringLiteral("system.json")),
                      policyDocument("grant", "deny",
                                     {{"audited-builtin", "global-menu", "windows.activate",
                                       "deny"}})));
    QVERIFY(writeFile(file(QStringLiteral("user.json")),
                      policyDocument("grant", "deny", {})));
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles(
        {file(QStringLiteral("system.json")), file(QStringLiteral("user.json"))});
    QVERIFY2(merged.ok, qPrintable(merged.error));
    const CapabilityRule *kept = ruleFor(merged.policy, PackageTrust::AuditedBuiltin,
                                         QStringLiteral("global-menu"),
                                         Capability::WindowActivate);
    QVERIFY(kept != nullptr);
    QVERIFY(kept->disposition == CapabilityDisposition::Deny);
}

void PolicyMergeTest::theHighestLayerSuppliesTheDefaults()
{
    QVERIFY(writeFile(file(QStringLiteral("a.json")), policyDocument("grant", "deny", {})));
    QVERIFY(writeFile(file(QStringLiteral("b.json")), policyDocument("deny", "deny", {})));
    QVERIFY(writeFile(file(QStringLiteral("c.json")), policyDocument("deny", "grant", {})));
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles(
        {file(QStringLiteral("a.json")), file(QStringLiteral("b.json")),
         file(QStringLiteral("c.json"))});
    QVERIFY2(merged.ok, qPrintable(merged.error));
    QVERIFY(merged.policy.auditedBuiltinDefault == CapabilityDisposition::Deny);
    QVERIFY(merged.policy.thirdPartyDefault == CapabilityDisposition::Grant);
}

void PolicyMergeTest::anUnreadableOrInvalidOverrideIsSkippedAndReported()
{
    QVERIFY(writeFile(file(QStringLiteral("system.json")),
                      policyDocument("grant", "deny",
                                     {{"third-party", "*", "applications.launch", "deny"}})));
    QVERIFY(writeFile(file(QStringLiteral("broken.json")), QByteArray("{ nope")));
    QVERIFY(writeFile(file(QStringLiteral("empty-object.json")), QByteArray("{}")));
    QStringList skipped;
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles(
        {file(QStringLiteral("system.json")), file(QStringLiteral("broken.json")),
         file(QStringLiteral("missing.json")), file(QStringLiteral("empty-object.json"))},
        &skipped);
    QVERIFY2(merged.ok, qPrintable(merged.error));
    QCOMPARE(skipped.size(), 3);
    QVERIFY(skipped.join(QLatin1Char('\n')).contains(QStringLiteral("broken.json")));
    QVERIFY(skipped.join(QLatin1Char('\n')).contains(QStringLiteral("missing.json")));
    QCOMPARE(merged.policy.rules.size(), 1);
    QVERIFY(merged.policy.thirdPartyDefault == CapabilityDisposition::Deny);
}

void PolicyMergeTest::anInvalidBaseFailsTheLoad()
{
    QVERIFY(writeFile(file(QStringLiteral("system.json")), QByteArray("{ nope")));
    QVERIFY(writeFile(file(QStringLiteral("user.json")), policyDocument("grant", "deny", {})));
    QStringList skipped;
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles(
        {file(QStringLiteral("system.json")), file(QStringLiteral("user.json"))}, &skipped);
    QVERIFY(!merged.ok);
    QVERIFY(merged.error.contains(QStringLiteral("system.json")));
}

void PolicyMergeTest::noFileIsAnError()
{
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles({});
    QVERIFY(!merged.ok);
    QVERIFY(merged.error.contains(QStringLiteral("no policy file")));
}

void PolicyMergeTest::aSingleFileMatchesFromFile()
{
    const QString shipped = QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json");
    const CapabilityPolicyLoadResult single = CapabilityPolicyLoader::fromFile(shipped);
    QVERIFY2(single.ok, qPrintable(single.error));
    const CapabilityPolicyLoadResult merged = CapabilityPolicyLoader::fromFiles({shipped});
    QVERIFY2(merged.ok, qPrintable(merged.error));
    QVERIFY(merged.policy.rules == single.policy.rules);
    QVERIFY(merged.policy.auditedBuiltinDefault == single.policy.auditedBuiltinDefault);
    QVERIFY(merged.policy.thirdPartyDefault == single.policy.thirdPartyDefault);
    // The shipped policy layered over itself is itself: nothing is duplicated.
    const CapabilityPolicyLoadResult twice = CapabilityPolicyLoader::fromFiles({shipped, shipped});
    QVERIFY2(twice.ok, qPrintable(twice.error));
    QVERIFY(twice.policy.rules == single.policy.rules);
}

QTEST_GUILESS_MAIN(PolicyMergeTest)
#include "tst_policy_merge.moc"
