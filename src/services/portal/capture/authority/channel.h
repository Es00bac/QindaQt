// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "packet.h"
#include <QDBusConnection>
#include <QObject>
#include <QTimer>
#include <functional>
#include <vector>
#include <utility>
class QSocketNotifier;
namespace QindaQt::Services::Portal::CaptureAuthority {
struct ReceivedPacket {
    Packet packet;
    std::vector<int> fds;
    ReceivedPacket(Packet value, std::vector<int> owned) : packet(std::move(value)), fds(std::move(owned)) {}
    ReceivedPacket(ReceivedPacket &&other) noexcept : packet(std::move(other.packet)), fds(std::exchange(other.fds, {})) {}
    ReceivedPacket(const ReceivedPacket &) = delete;
    ~ReceivedPacket();
    std::vector<int> takeFds() { return std::exchange(fds, {}); }
};
enum class Role { Broker, Helper };
// Same-thread channel owning its supplied fd and compositor pidfd. Callbacks
// borrow their owner, must not destroy this channel synchronously, and finish
// before the next packet. Received FDs close unless explicitly taken. There is
// no outgoing queue: backpressure, malformed credentials or lineage loss are
// terminal. Every operation/read-through rechecks actual selected owner/PIDFD.
class Channel final : public QObject {
public:
    Channel(int ownedFd, Role, QDBusConnection, QObject *parent = nullptr);
    ~Channel() override;
    bool start(std::function<void(ReceivedPacket &&)> receive, std::function<void()> lost);
    bool live() const;
    bool available() const { return m_fd >= 0 && !m_failed; }
    bool send(Wire::Message, quint64 job, const QByteArray &payload = {});
    void reconcile();
    void stop();
    QString owner() const { return m_owner; }
    quint64 generation() const { return m_generation; }
    quint64 job() const { return m_job; }
private:
    bool identityLive() const;
    bool transmit(const Packet &);
    void fail();
    int m_fd = -1, m_pidfd = -1;
    qint64 m_pid = 0;
    Role m_role;
    QDBusConnection m_bus;
    QString m_owner;
    QSocketNotifier *m_notifier = nullptr;
    QTimer m_handshake;
    quint64 m_generation = 0, m_job = 0, m_highest = 0;
    bool m_ready = false, m_reading = false, m_failed = false;
    bool m_parent = false, m_consent = false, m_granted = false, m_revoking = false;
    std::function<void(ReceivedPacket &&)> m_receive;
    std::function<void()> m_lost;
};
}
