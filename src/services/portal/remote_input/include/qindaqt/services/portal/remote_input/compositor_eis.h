// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QHash>
#include <QObject>
#include <functional>

namespace QindaQt::Services::Portal::RemoteInput {
// Standard portal device bits. They equal the capability bits of the fork's
// org.qindaqt.KWin.EIS.RemoteDesktop.connectToEIS, so no translation table exists.
enum DeviceType : quint32 { Keyboard = 1, Pointer = 2, Touchscreen = 4 };
inline constexpr quint32 kAllDeviceTypes = Keyboard | Pointer | Touchscreen;

// Client of the selected compositor's EIS RemoteDesktop object. Every call
// targets the exact unique owner returned by the borrowed provider (production:
// PortalSessionBinding::compositorOwner), never a well-known name, frontend
// option or application-supplied socket. The provider must outlive this
// same-thread object. Returned FDs are owned by the receiver of opened().
// AGENT-CONTRACT: fork src/plugins/eis admits only the current portal backend
// owner, refuses while natively locked and destroys contexts on lock.
class CompositorEis final : public QObject {
    Q_OBJECT
public:
    using OwnerProvider = std::function<QString()>;
    CompositorEis(QDBusConnection, OwnerProvider, QObject *parent = nullptr);
    ~CompositorEis() override;
    // Returns 0 without a selected compositor or for invalid device bits.
    // Otherwise exactly one opened/failed follows within 5 s unless cancelled.
    quint64 open(quint32 deviceTypes);
    // A cancelled ticket's late success is disconnected and never published.
    void cancel(quint64 ticket);
    // Idempotent; a replaced compositor already destroyed the old context.
    void close(const QString &compositor, int cookie);
    // InputCapture manager/object call on the selected owner. Returns false
    // without one; otherwise `done` runs once with the reply or error message
    // and the owner it was sent to, unless this object is destroyed first.
    using Done = std::function<void(const QDBusMessage &reply, const QString &owner)>;
    bool call(const QString &path, const QString &interface, const QString &member,
              const QVariantList &arguments, Done done);
    QString compositor() const { return currentOwner(); }
    QDBusConnection connection() const { return m_bus; }
Q_SIGNALS:
    void opened(quint64 ticket, const QDBusUnixFileDescriptor &fd,
                const QString &compositor, int cookie);
    void failed(quint64 ticket);
private:
    QString currentOwner() const;
    QDBusConnection m_bus;
    OwnerProvider m_owner;
    QHash<quint64, QString> m_pending;
    quint64 m_sequence = 0;
};
} // namespace QindaQt::Services::Portal::RemoteInput
