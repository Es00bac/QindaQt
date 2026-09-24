// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_policy/notification_application_policy.h"

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

#include <QMetaType>
#include <QRegularExpression>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Services::NotificationPresentationPolicy {
namespace {

constexpr auto MutedField = "muted";
constexpr auto SoundEnabledField = "soundEnabled";
const QRegularExpression DesktopIdPattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,254}$"));

bool validPolicies(const PerApplicationNotificationPolicies &policies) noexcept
{
    qsizetype storedRuleCount = 0;
    for (auto it = policies.cbegin(); it != policies.cend(); ++it) {
        if (!NotificationApplicationPolicy::isCanonicalDesktopId(it.key())) {
            return false;
        }
        if (it->muted || it->soundEnabled) {
            ++storedRuleCount;
        }
    }
    return storedRuleCount <= NotificationApplicationPolicy::MaximumApplicationRules;
}

} // namespace

NotificationApplicationPolicy::NotificationApplicationPolicy(QObject *parent)
    : QObject(parent)
{
}

const PerApplicationNotificationPolicies &
NotificationApplicationPolicy::policies() const noexcept
{
    return m_policies;
}

PerApplicationNotificationPolicy
NotificationApplicationPolicy::policyForDesktopEntry(const QString &desktopEntry) const noexcept
{
    const QString id = canonicalDesktopEntryId(desktopEntry);
    if (id.isEmpty()) {
        return {};
    }
    return m_policies.value(id);
}

bool NotificationApplicationPolicy::allowsPopup(
    const NotificationPresentation::PresentationNotification &notification) const noexcept
{
    // The desktop-entry hint is producer supplied, so this is a user-facing
    // preference match, never an authorization boundary. Missing/unknown ids
    // retain normal presentation behavior for compatibility.
    return !policyForDesktopEntry(notification.desktopEntry).muted;
}

bool NotificationApplicationPolicy::allowsSound(
    const NotificationPresentation::PresentationNotification &notification) const noexcept
{
    const PerApplicationNotificationPolicy policy =
        policyForDesktopEntry(notification.desktopEntry);
    return !policy.muted && policy.soundEnabled;
}

PerApplicationPolicyDecodeResult
NotificationApplicationPolicy::decodeSettingsValue(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QVariantMap) {
        return {std::nullopt,
                QStringLiteral("notification policies must be a Settings object")};
    }
    const QVariantMap encoded = value.toMap();
    if (encoded.size() > MaximumApplicationRules) {
        return {std::nullopt,
                QStringLiteral("notification policy entry limit is exceeded")};
    }

    PerApplicationNotificationPolicies decoded;
    for (auto it = encoded.cbegin(); it != encoded.cend(); ++it) {
        if (!isCanonicalDesktopId(it.key()) ||
            it.value().metaType().id() != QMetaType::QVariantMap) {
            return {std::nullopt,
                    QStringLiteral("notification policy has an invalid desktop id or record")};
        }
        const QVariantMap record = it.value().toMap();
        if (record.size() != 2 || !record.contains(QLatin1String(MutedField)) ||
            !record.contains(QLatin1String(SoundEnabledField)) ||
            record.value(QLatin1String(MutedField)).metaType().id() != QMetaType::Bool ||
            record.value(QLatin1String(SoundEnabledField)).metaType().id() != QMetaType::Bool) {
            return {std::nullopt,
                    QStringLiteral("notification policy fields must be exact Booleans")};
        }
        PerApplicationNotificationPolicy policy{
            .muted = record.value(QLatin1String(MutedField)).toBool(),
            .soundEnabled = record.value(QLatin1String(SoundEnabledField)).toBool(),
        };
        if (!policy.muted && !policy.soundEnabled) {
            return {std::nullopt,
                    QStringLiteral("default notification policy records must be omitted")};
        }
        decoded.insert(it.key(), policy);
    }
    return {std::move(decoded), {}};
}

bool NotificationApplicationPolicy::encodeSettingsValue(
    const PerApplicationNotificationPolicies &policies,
    QVariantMap *value, QString *error)
{
    if (!value) {
        if (error) {
            *error = QStringLiteral("notification policy output is missing");
        }
        return false;
    }
    if (!validPolicies(policies)) {
        if (error) {
            *error = QStringLiteral("notification policy map is invalid or exceeds its limit");
        }
        return false;
    }
    QVariantMap encoded;
    for (auto it = policies.cbegin(); it != policies.cend(); ++it) {
        if (!it->muted && !it->soundEnabled) {
            continue;
        }
        encoded.insert(it.key(), QVariantMap{
            {QLatin1String(MutedField), it->muted},
            {QLatin1String(SoundEnabledField), it->soundEnabled},
        });
    }
    *value = std::move(encoded);
    if (error) {
        error->clear();
    }
    return true;
}

bool NotificationApplicationPolicy::isCanonicalDesktopId(
    const QString &desktopId) noexcept
{
    return !desktopId.isEmpty() && desktopId.size() <= MaximumDesktopIdCodeUnits &&
           !desktopId.endsWith(QLatin1String(".desktop")) &&
           !desktopId.contains(QChar::Null) && DesktopIdPattern.match(desktopId).hasMatch();
}

QString NotificationApplicationPolicy::canonicalDesktopEntryId(
    const QString &desktopEntry)
{
    QString id = desktopEntry;
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return isCanonicalDesktopId(id) ? id : QString{};
}

bool NotificationApplicationPolicy::setPolicies(
    const PerApplicationNotificationPolicies &policies)
{
    if (!validPolicies(policies)) {
        return false;
    }
    PerApplicationNotificationPolicies normalized = policies;
    for (auto it = normalized.begin(); it != normalized.end();) {
        if (!it->muted && !it->soundEnabled) {
            it = normalized.erase(it);
        } else {
            ++it;
        }
    }
    if (m_policies == normalized) {
        return true;
    }
    m_policies = std::move(normalized);
    Q_EMIT policiesChanged();
    return true;
}

bool NotificationApplicationPolicy::setSettingsValue(const QVariant &value,
                                                       QString *error)
{
    const PerApplicationPolicyDecodeResult decoded = decodeSettingsValue(value);
    if (!decoded.ok()) {
        if (error) {
            *error = decoded.error;
        }
        return false;
    }
    if (!setPolicies(*decoded.policies)) {
        if (error) {
            *error = QStringLiteral("notification policy map is invalid");
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace QindaQt::Services::NotificationPresentationPolicy
