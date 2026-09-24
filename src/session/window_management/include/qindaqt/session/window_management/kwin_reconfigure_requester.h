// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

#include <QString>

class QDBusServiceWatcher;

namespace QindaQt::Session::WindowManagement {

// Seam between the bridge and KWin. Calls stay on the bridge's event-loop
// thread; for each nonzero request id, the requester emits one completion with
// the exact unique owner it addressed, or an actionable error. ownerChanged
// carries the current unique owner, or empty when KWin is absent. The bridge
// borrows this object, which must outlive it.
class KWinReconfigureRequester : public QObject {
    Q_OBJECT
public:
    explicit KWinReconfigureRequester(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~KWinReconfigureRequester() override = default;
    virtual void requestReconfigure(quint64 requestId) = 0;

Q_SIGNALS:
    void reconfigureFinished(quint64 requestId, const QString &owner,
                             const QString &errorMessage);
    void ownerChanged(const QString &newOwner);
};

// Calls `org.kde.KWin /KWin org.kde.KWin.reconfigure` asynchronously with a
// five-second timeout to the exact watched unique owner. A vanished or slow
// compositor never blocks the session process; completion reports the error.
class DBusKWinReconfigureRequester final : public KWinReconfigureRequester {
    Q_OBJECT
public:
    explicit DBusKWinReconfigureRequester(QDBusConnection bus, QObject *parent = nullptr);
    void requestReconfigure(quint64 requestId) override;

    [[nodiscard]] int requestCount() const noexcept { return m_requests; }
    [[nodiscard]] const QString &currentOwner() const noexcept { return m_owner; }

private:
    QDBusConnection m_bus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QString m_owner;
    int m_requests = 0;
};

} // namespace QindaQt::Session::WindowManagement
