// SPDX-License-Identifier: LGPL-3.0-or-later
#include "portal_session_lifetime.h"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QRegularExpression>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
namespace { constexpr auto Service = "org.qindaqt.Portal1"; constexpr auto Path = "/org/qindaqt/Portal1"; }
PortalSessionLifetime::PortalSessionLifetime(QObject *parent)
    : QObject(parent), child_(QStringLiteral("portal"), {}) {
    retry_.setInterval(100);
    connect(&retry_, &QTimer::timeout, this, &PortalSessionLifetime::attach);
}
PortalSessionLifetime::~PortalSessionLifetime() { stop(); }
void PortalSessionLifetime::start(const QString &program, const QString &display) {
    static const QRegularExpression canonical(QStringLiteral("^qindaqt-(0|[1-9][0-9]{0,3})$"));
    if (bus_ || program.isEmpty() || !canonical.match(display).hasMatch()
        || display.mid(8).toUInt() > 4095) return;
    connectionName_ = QStringLiteral("qindaqt-portal-session-") + QUuid::createUuid().toString(QUuid::Id128);
    bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName_));
    if (!bus_->isConnected()) { stop(); return; }
    display_ = display;
    watcher_ = std::make_unique<QDBusServiceWatcher>(QString::fromLatin1(Service), *bus_, QDBusServiceWatcher::WatchForOwnerChange);
    connect(watcher_.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &newOwner) {
            // AGENT-GUARD: queued loss/arrival may follow a fresh owner lookup.
            // Preserve an already selected identical owner instead of replaying
            // its Attach call; loss still retires every in-flight generation.
            if (!newOwner.isEmpty() && newOwner == owner_) return;
            ++generation_; pending_ = false; owner_.clear(); attempts_ = 0;
            retry_.start();
            if (!newOwner.isEmpty()) attach();
        });
    child_.start(program);
    retry_.start(); attach();
}
void PortalSessionLifetime::attach() {
    if (!bus_ || pending_) return;
    if (++attempts_ > 30) { retry_.stop(); return; }
    const QDBusReply<QString> owner = bus_->interface()->serviceOwner(QString::fromLatin1(Service));
    if (!owner.isValid() || !owner.value().startsWith(QLatin1Char(':'))) return;
    const QDBusReply<uint> uid = bus_->interface()->serviceUid(owner.value());
    if (!uid.isValid() || uid.value() != static_cast<uint>(geteuid())) { retry_.stop(); return; }
    owner_ = owner.value();
    auto request = QDBusMessage::createMethodCall(owner_, QString::fromLatin1(Path), QString::fromLatin1(Service), QStringLiteral("AttachSessionWithDisplay"));
    request.setArguments({display_}); request.setAutoStartService(false);
    pending_ = true;
    const auto generation = generation_;
    auto *reply = new QDBusPendingCallWatcher(bus_->asyncCall(request, 500), this);
    connect(reply, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *done) {
        const QDBusPendingReply<bool> result = *done;
        done->deleteLater();
        if (!bus_ || generation != generation_) return;
        pending_ = false;
        // AGENT-GUARD: a reply does not admit the display. It only stops this
        // bounded transport retry; replacement starts fresh against its owner.
        if (!result.isError() && result.value()) retry_.stop();
    });
}
void PortalSessionLifetime::stop() noexcept {
    ++generation_; pending_ = false; retry_.stop(); watcher_.reset();
    if (bus_) { QDBusConnection::disconnectFromBus(connectionName_); bus_.reset(); }
    child_.stop(); display_.clear(); owner_.clear(); attempts_ = 0;
}
}
