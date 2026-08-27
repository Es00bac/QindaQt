// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Shell {

// This facade deliberately carries no notification records or operation APIs.
// It is the complete authority offered to the built-in panel entry.
class NotificationCenterAppletAccess final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool centerOpen READ centerOpen NOTIFY centerOpenChanged)
    Q_PROPERTY(bool doNotDisturbEnabled READ doNotDisturbEnabled
                   NOTIFY doNotDisturbEnabledChanged)
    Q_PROPERTY(bool privatePresentationAllowed READ privatePresentationAllowed
                   NOTIFY privatePresentationAllowedChanged)

public:
    explicit NotificationCenterAppletAccess(QObject *parent = nullptr);

    [[nodiscard]] bool centerOpen() const noexcept;
    [[nodiscard]] bool doNotDisturbEnabled() const noexcept;
    [[nodiscard]] bool privatePresentationAllowed() const noexcept;
    Q_INVOKABLE void toggle();

    // Shell composition mirrors authoritative policy/presentation state. QML
    // cannot call either publisher through the meta-object boundary.
    void publishCenterOpen(bool open);
    void publishDoNotDisturbEnabled(bool enabled);
    void publishPrivatePresentationAllowed(bool allowed);

Q_SIGNALS:
    void toggleRequested();
    void centerOpenChanged();
    void doNotDisturbEnabledChanged();
    void privatePresentationAllowedChanged();

private:
    bool m_centerOpen = false;
    bool m_doNotDisturbEnabled = false;
    bool m_privatePresentationAllowed = false;
};

} // namespace QindaQt::Shell
