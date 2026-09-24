// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::NotificationPresentation {
struct PresentationNotification;
}

namespace QindaQt::Services::NotificationPresentationPolicy {

inline constexpr auto NotificationPoliciesSettingsKey =
    "services.notificationPolicies";

struct PerApplicationNotificationPolicy final {
    bool muted = false;
    // Sound is opt-in because the shell has historically presented notifications
    // silently. A missing record therefore preserves the existing behavior.
    bool soundEnabled = false;

    [[nodiscard]] bool operator==(const PerApplicationNotificationPolicy &) const = default;
};

using PerApplicationNotificationPolicies =
    QMap<QString, PerApplicationNotificationPolicy>;

struct PerApplicationPolicyDecodeResult final {
    std::optional<PerApplicationNotificationPolicies> policies;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return policies.has_value(); }
};

// AGENT-CONTRACT: this QObject owns its confirmed map and is thread-confined;
// an optional QObject parent owns its lifetime. The Settings route and shell
// bridge use the same strict codec, so malformed input never publishes partial
// policy authority.
class NotificationApplicationPolicy final : public QObject {
    Q_OBJECT

public:
    static constexpr qsizetype MaximumApplicationRules = 256;
    static constexpr qsizetype MaximumDesktopIdCodeUnits = 255;

    explicit NotificationApplicationPolicy(QObject *parent = nullptr);

    [[nodiscard]] const PerApplicationNotificationPolicies &policies() const noexcept;
    [[nodiscard]] PerApplicationNotificationPolicy policyForDesktopEntry(
        const QString &desktopEntry) const noexcept;
    [[nodiscard]] bool allowsPopup(
        const NotificationPresentation::PresentationNotification &notification) const noexcept;
    [[nodiscard]] bool allowsSound(
        const NotificationPresentation::PresentationNotification &notification) const noexcept;

    // The Settings value is an object keyed by the canonical desktop-entry id
    // without the `.desktop` suffix. Each record has exactly two Boolean
    // fields: `muted` and `soundEnabled`; rules with both values false are
    // omitted from storage.
    [[nodiscard]] static PerApplicationPolicyDecodeResult decodeSettingsValue(
        const QVariant &value);
    [[nodiscard]] static bool encodeSettingsValue(
        const PerApplicationNotificationPolicies &policies,
        QVariantMap *value, QString *error = nullptr);
    [[nodiscard]] static bool isCanonicalDesktopId(const QString &desktopId) noexcept;
    // Notification producers commonly include the suffix while the shared
    // application catalog's id omits it. This returns the one matching id or
    // an empty string for missing/malformed hints.
    [[nodiscard]] static QString canonicalDesktopEntryId(
        const QString &desktopEntry);

    // Rejects an entire update on malformed input; false preserves the prior
    // map and emits no signal. Calls must run on this object's thread.
    [[nodiscard]] bool setPolicies(
        const PerApplicationNotificationPolicies &policies);
    // Uses the exact persisted codec. False preserves the prior map and reports
    // the reason through `error` when supplied.
    [[nodiscard]] bool setSettingsValue(const QVariant &value, QString *error = nullptr);

Q_SIGNALS:
    void policiesChanged();

private:
    PerApplicationNotificationPolicies m_policies;
};

} // namespace QindaQt::Services::NotificationPresentationPolicy
