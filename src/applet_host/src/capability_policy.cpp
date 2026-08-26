// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy.h"

#include <QRegularExpression>
#include <QSet>

namespace QindaQt::AppletHost {
namespace {

bool isValidPackageId(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$)"));
    return pattern.match(value).hasMatch();
}

QString defaultReason(PackageTrust trust, CapabilityDisposition disposition)
{
    return QStringLiteral("%1 policy default for %2 packages")
        .arg(toString(disposition), toString(trust));
}

} // namespace

bool CapabilityRule::matches(const PackageIdentity &package,
                             QindaQt::Applets::Capability requested) const
{
    return trust == package.trust && capability == requested
        && (packageId.isEmpty() || packageId == package.packageId);
}

bool CapabilityRule::isPackageSpecific() const
{
    return !packageId.isEmpty();
}

bool CapabilityDecision::granted() const
{
    return disposition == CapabilityDisposition::Grant;
}

bool PolicyValidation::isValid() const
{
    return errors.isEmpty();
}

QString PolicyValidation::summary() const
{
    return errors.join(QStringLiteral("; "));
}

PolicyValidation CapabilityPolicy::validate() const
{
    PolicyValidation validation;
    if (schemaVersion != CapabilityPolicySchemaVersion) {
        validation.errors.append(QStringLiteral("schemaVersion must be 1"));
    }

    QSet<QString> selectors;
    for (const CapabilityRule &rule : rules) {
        if (!rule.packageId.isEmpty() && !isValidPackageId(rule.packageId)) {
            validation.errors.append(
                QStringLiteral("rule packageId '%1' is invalid").arg(rule.packageId));
        }
        if (rule.reason.trimmed().isEmpty()) {
            validation.errors.append(QStringLiteral("every capability rule requires a reason"));
        }
        const QString selector = QStringLiteral("%1|%2|%3")
                                     .arg(toString(rule.trust),
                                          rule.packageId,
                                          QindaQt::Applets::toString(rule.capability));
        if (selectors.contains(selector)) {
            validation.errors.append(
                QStringLiteral("duplicate capability rule selector: %1").arg(selector));
        }
        selectors.insert(selector);
    }
    return validation;
}

CapabilityEvaluation CapabilityPolicy::evaluate(
    const QindaQt::Applets::AppletManifest &manifest,
    const PackageIdentity &package) const
{
    const PolicyValidation policyValidation = validate();
    if (!policyValidation.isValid()) {
        return {.ok = false,
                .decisions = {},
                .error = QStringLiteral("Invalid capability policy: %1")
                             .arg(policyValidation.summary())};
    }
    const QindaQt::Applets::ManifestValidation manifestValidation = manifest.validate();
    if (!manifestValidation.isValid()) {
        return {.ok = false,
                .decisions = {},
                .error = QStringLiteral("Invalid applet manifest: %1")
                             .arg(manifestValidation.summary())};
    }
    if (package.packageId != manifest.id) {
        return {.ok = false,
                .decisions = {},
                .error = QStringLiteral("Package identity does not match manifest id")};
    }

    QVector<CapabilityDecision> decisions;
    decisions.reserve(manifest.capabilities.size());
    for (const QindaQt::Applets::Capability capability : manifest.capabilities) {
        const CapabilityRule *rule = findRule(package, capability);
        if (rule != nullptr) {
            decisions.append({capability,
                              rule->disposition,
                              DecisionBasis::SpecificRule,
                              rule->reason});
            continue;
        }
        const CapabilityDisposition defaultDisposition =
            package.trust == PackageTrust::AuditedBuiltin ? auditedBuiltinDefault
                                                          : thirdPartyDefault;
        decisions.append({capability,
                          defaultDisposition,
                          DecisionBasis::TrustDefault,
                          defaultReason(package.trust, defaultDisposition)});
    }
    return {.ok = true, .decisions = decisions, .error = {}};
}

const CapabilityRule *CapabilityPolicy::findRule(
    const PackageIdentity &package,
    QindaQt::Applets::Capability capability) const
{
    const CapabilityRule *wildcard = nullptr;
    for (const CapabilityRule &rule : rules) {
        if (!rule.matches(package, capability)) {
            continue;
        }
        if (rule.isPackageSpecific()) {
            return &rule;
        }
        wildcard = &rule;
    }
    return wildcard;
}

QString toString(CapabilityDisposition value)
{
    switch (value) {
    case CapabilityDisposition::Grant:
        return QStringLiteral("grant");
    case CapabilityDisposition::Deny:
        return QStringLiteral("deny");
    }
    return {};
}

QString toString(DecisionBasis value)
{
    switch (value) {
    case DecisionBasis::SpecificRule:
        return QStringLiteral("specific-rule");
    case DecisionBasis::TrustDefault:
        return QStringLiteral("trust-default");
    }
    return {};
}

std::optional<CapabilityDisposition> capabilityDispositionFromString(const QString &value)
{
    if (value == QLatin1String("grant")) {
        return CapabilityDisposition::Grant;
    }
    if (value == QLatin1String("deny")) {
        return CapabilityDisposition::Deny;
    }
    return std::nullopt;
}

std::optional<DecisionBasis> decisionBasisFromString(const QString &value)
{
    if (value == QLatin1String("specific-rule")) {
        return DecisionBasis::SpecificRule;
    }
    if (value == QLatin1String("trust-default")) {
        return DecisionBasis::TrustDefault;
    }
    return std::nullopt;
}

std::optional<PackageTrust> packageTrustFromString(const QString &value)
{
    if (value == QLatin1String("audited-builtin")) {
        return PackageTrust::AuditedBuiltin;
    }
    if (value == QLatin1String("third-party")) {
        return PackageTrust::ThirdParty;
    }
    return std::nullopt;
}

} // namespace QindaQt::AppletHost
