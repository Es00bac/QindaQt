// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <functional>

namespace QindaQt::Services::NotificationPresentationPolicy {

class NotificationPrivacyPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool privatePresentationAllowed READ privatePresentationAllowed NOTIFY
                   privatePresentationAllowedChanged)

public:
    // AGENT-CONTRACT: Same-thread policy; the observer owns evidence. Production
    // supplies a read-only admission dependency that outlives this object and
    // neither dispatches events nor mutates/deletes consumers. Every disclosure
    // read rechecks it: queued owner-loss signals cannot retain permission.
    // An admission exception denies access. The signal-only constructor remains
    // available for source-compatible deterministic/non-platform consumers.
    using Admission = std::function<bool()>;
    explicit NotificationPrivacyPolicy(QObject *parent = nullptr);
    explicit NotificationPrivacyPolicy(Admission admission, QObject *parent = nullptr);

    [[nodiscard]] bool privatePresentationAllowed() const noexcept;

    // This is intentionally a C++ method rather than a Q_PROPERTY writer. QML
    // presentation and applets may observe the decision but cannot grant
    // themselves access to private notification content.
    void setPrivatePresentationAllowed(bool allowed);

Q_SIGNALS:
    void privatePresentationAllowedChanged(bool allowed);

private:
    bool m_privatePresentationAllowed = false;
    Admission m_admission;
};

} // namespace QindaQt::Services::NotificationPresentationPolicy
