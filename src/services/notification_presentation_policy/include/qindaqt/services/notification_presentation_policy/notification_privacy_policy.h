// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Services::NotificationPresentationPolicy {

class NotificationPrivacyPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool privatePresentationAllowed READ privatePresentationAllowed NOTIFY
                   privatePresentationAllowedChanged)

public:
    // AGENT-CONTRACT: This thread-confined policy contains only the current
    // fail-closed decision. A platform lock observer owns the evidence and
    // must set true only after authenticating an unlocked compositor lineage.
    explicit NotificationPrivacyPolicy(QObject *parent = nullptr);

    [[nodiscard]] bool privatePresentationAllowed() const noexcept;

    // This is intentionally a C++ method rather than a Q_PROPERTY writer. QML
    // presentation and applets may observe the decision but cannot grant
    // themselves access to private notification content.
    void setPrivatePresentationAllowed(bool allowed);

Q_SIGNALS:
    void privatePresentationAllowedChanged(bool allowed);

private:
    bool m_privatePresentationAllowed = false;
};

} // namespace QindaQt::Services::NotificationPresentationPolicy
