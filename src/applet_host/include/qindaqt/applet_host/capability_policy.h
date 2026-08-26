// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/host_selection.h"

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/applets/manifest_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::AppletHost {

inline constexpr int CapabilityPolicySchemaVersion = 1;

enum class CapabilityDisposition {
    Grant,
    Deny,
};

enum class DecisionBasis {
    SpecificRule,
    TrustDefault,
};

struct CapabilityRule final {
    PackageTrust trust = PackageTrust::ThirdParty;
    QString packageId;
    QindaQt::Applets::Capability capability = QindaQt::Applets::Capability::SettingsRead;
    CapabilityDisposition disposition = CapabilityDisposition::Deny;
    QString reason;

    [[nodiscard]] bool matches(const PackageIdentity &package,
                               QindaQt::Applets::Capability requested) const;
    [[nodiscard]] bool isPackageSpecific() const;

    bool operator==(const CapabilityRule &) const = default;
};

struct CapabilityDecision final {
    QindaQt::Applets::Capability capability = QindaQt::Applets::Capability::SettingsRead;
    CapabilityDisposition disposition = CapabilityDisposition::Deny;
    DecisionBasis basis = DecisionBasis::TrustDefault;
    QString reason;

    [[nodiscard]] bool granted() const;

    bool operator==(const CapabilityDecision &) const = default;
};

struct PolicyValidation final {
    QStringList errors;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString summary() const;
};

struct CapabilityEvaluation final {
    bool ok = false;
    QVector<CapabilityDecision> decisions;
    QString error;
};

class CapabilityPolicy final {
public:
    int schemaVersion = CapabilityPolicySchemaVersion;
    CapabilityDisposition auditedBuiltinDefault = CapabilityDisposition::Deny;
    CapabilityDisposition thirdPartyDefault = CapabilityDisposition::Deny;
    QVector<CapabilityRule> rules;

    [[nodiscard]] PolicyValidation validate() const;

    // AGENT-CONTRACT: Grant means a mediator may expose the named interface. It
    // never grants OS resources itself, bypasses host isolation, or proves that
    // a portal/service adapter exists.
    [[nodiscard]] CapabilityEvaluation evaluate(
        const QindaQt::Applets::AppletManifest &manifest,
        const PackageIdentity &package) const;

private:
    [[nodiscard]] const CapabilityRule *findRule(
        const PackageIdentity &package,
        QindaQt::Applets::Capability capability) const;
};

[[nodiscard]] QString toString(CapabilityDisposition value);
[[nodiscard]] QString toString(DecisionBasis value);
[[nodiscard]] std::optional<CapabilityDisposition> capabilityDispositionFromString(
    const QString &value);
[[nodiscard]] std::optional<DecisionBasis> decisionBasisFromString(const QString &value);
[[nodiscard]] std::optional<PackageTrust> packageTrustFromString(const QString &value);

} // namespace QindaQt::AppletHost
