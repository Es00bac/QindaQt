// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Shell {

// This facade deliberately carries no notification records or operation APIs.
// It is the complete authority offered to the built-in panel entry.
class NotificationCenterAppletAccess final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool centerOpen READ centerOpen NOTIFY centerOpenChanged)

public:
    explicit NotificationCenterAppletAccess(QObject *parent = nullptr);

    [[nodiscard]] bool centerOpen() const noexcept;
    Q_INVOKABLE void toggle();

    // Shell composition mirrors the authoritative presentation controller;
    // QML cannot call this method through the meta-object boundary.
    void publishCenterOpen(bool open);

Q_SIGNALS:
    void toggleRequested();
    void centerOpenChanged();

private:
    bool m_centerOpen = false;
};

} // namespace QindaQt::Shell
