// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Services::Clipboard {

// Clipboard-private composition of public ordinary attachment and native lock
// receipts (ADR-0358). Same-thread only. PID is the independently verified
// ordinary Wayland connection's SO_PEERCRED, never an environment/state value.
// The first selected Session1 owner and compositor incarnation are pinned;
// revocation/stop requires a fresh composition, not a replacement-owner retry.
class NativeClipboardLockObserver final : public QObject {
    Q_OBJECT
public:
    NativeClipboardLockObserver(QDBusConnection bus, qint64 compositorPid,
                                QString runtimeDirectory, QString socketBasename,
                                QObject *parent = nullptr);
    ~NativeClipboardLockObserver() override;
    // Successful start installs observation; unlocked truth requires a fresh
    // targeted native nonce receipt and its authenticated empty method reply.
    bool start();
    void stop();
    [[nodiscard]] bool contentMayBeShown() const;

Q_SIGNALS:
    void contentMayBeShownChanged(bool allowed);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::Clipboard
