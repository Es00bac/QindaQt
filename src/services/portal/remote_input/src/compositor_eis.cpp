// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/remote_input/compositor_eis.h>
#include <QDBusMessage>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr auto kPath = CompositorNames::eisRemoteDesktopPath;
constexpr auto kInterface = CompositorNames::eisRemoteDesktopInterface;
QDBusMessage compositorCall(const QString &owner, const QString &member) {
    auto call = QDBusMessage::createMethodCall(owner, QLatin1String(kPath), QLatin1String(kInterface), member);
    call.setAutoStartService(false);
    return call;
}
}
CompositorEis::CompositorEis(QDBusConnection bus, OwnerProvider owner, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_owner(std::move(owner)) {}
CompositorEis::~CompositorEis() = default;
QString CompositorEis::currentOwner() const {
    const QString owner = m_owner ? m_owner() : QString{};
    return owner.startsWith(QLatin1Char(':')) ? owner : QString{};
}
quint64 CompositorEis::open(quint32 types) {
    const QString owner = currentOwner();
    if (owner.isEmpty() || !types || (types & ~kAllDeviceTypes)) return 0;
    auto call = compositorCall(owner, QStringLiteral("connectToEIS"));
    call << static_cast<int>(types);
    const quint64 ticket = ++m_sequence;
    m_pending.insert(ticket, owner);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, ticket, owner](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        const QDBusPendingReply<QDBusUnixFileDescriptor, int> reply = *finished;
        const bool wanted = m_pending.remove(ticket) > 0;
        const bool valid = !reply.isError() && reply.argumentAt<0>().isValid() && reply.argumentAt<1>() > 0;
        // A replaced compositor or retired ticket never receives a transport.
        if (valid && (!wanted || currentOwner() != owner)) close(owner, reply.argumentAt<1>());
        if (!wanted) return;
        if (valid && currentOwner() == owner)
            Q_EMIT opened(ticket, reply.argumentAt<0>(), owner, reply.argumentAt<1>());
        else Q_EMIT failed(ticket);
    });
    return ticket;
}
bool CompositorEis::call(const QString &path, const QString &interface, const QString &member,
                         const QVariantList &arguments, Done done) {
    const QString owner = currentOwner();
    if (owner.isEmpty()) return false;
    auto message = QDBusMessage::createMethodCall(owner, path, interface, member);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [owner, done = std::move(done)](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        done(finished->reply(), owner);
    });
    return true;
}
void CompositorEis::cancel(quint64 ticket) { m_pending.remove(ticket); }
void CompositorEis::close(const QString &compositor, int cookie) {
    if (cookie <= 0 || compositor.isEmpty() || currentOwner() != compositor) return;
    auto call = compositorCall(compositor, QStringLiteral("disconnect"));
    call << cookie;
    m_bus.send(call);
}
} // namespace QindaQt::Services::Portal::RemoteInput
