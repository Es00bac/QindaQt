// SPDX-License-Identifier: LGPL-3.0-or-later
#include "portal_session_lifetime.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
namespace { constexpr auto Service = "org.qindaqt.Portal1"; constexpr auto Path = "/org/qindaqt/Portal1"; constexpr auto Backend = "org.freedesktop.impl.portal.desktop.qindaqt"; }
PortalSessionLifetime::PortalSessionLifetime(QObject *parent)
    : PortalSessionLifetime(false, parent) {}
PortalSessionLifetime::PortalSessionLifetime(bool captureControl, QObject *parent)
    : QObject(parent),
      service_(captureControl ? QStringLiteral("org.qindaqt.PortalCapture1") : QString::fromLatin1(Service)),
      path_(captureControl ? QStringLiteral("/org/qindaqt/PortalCapture1") : QString::fromLatin1(Path)) {
    // AGENT-NOTE (ADR-0379): a bounded 100 ms transport retry of at most
    // thirty attempts per owner; owner and UID lookups are asynchronous, so the
    // tick never blocks the supervisor. Owner arrival re-arms it.
    retry_.setInterval(100);
    connect(&retry_, &QTimer::timeout, this, &PortalSessionLifetime::attach);
}
PortalSessionLifetime::~PortalSessionLifetime() { stop(); }
void PortalSessionLifetime::start(const QString &program, const QString &display) {
    if (program.isEmpty()) return;
    begin(program, display);
}
void PortalSessionLifetime::attachCapture(const QString &display) { begin({}, display); }
void PortalSessionLifetime::begin(const QString &program, const QString &display) {
    static const QRegularExpression canonical(QStringLiteral("^qindaqt-(0|[1-9][0-9]{0,3})$"));
    if (bus_ || !canonical.match(display).hasMatch()
        || display.mid(8).toUInt() > 4095) return;
    connectionName_ = QStringLiteral("qindaqt-portal-session-") + QUuid::createUuid().toString(QUuid::Id128);
    bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName_));
    if (!bus_->isConnected()) { stop(); return; }
    display_ = display;
    watcher_ = std::make_unique<QDBusServiceWatcher>(service_, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
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
    if (!program.isEmpty()) {
        // AGENT-CONTRACT: the .service descriptor and systemd unit are the
        // only backend launch authority. A second child races for Portal1
        // and leaves a failed user unit even when the session copy works.
        auto activation = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("StartServiceByName"));
        activation.setArguments({QString::fromLatin1(Backend), quint32(0)});
        bus_->asyncCall(activation, 2000);
    }
    retry_.start(); attach();
}
namespace {
QDBusMessage daemonCall(const char *method, const QString &argument) {
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QString::fromLatin1(method));
    message << argument;
    return message;
}
}
void PortalSessionLifetime::attach() {
    if (!bus_ || pending_) return;
    if (++attempts_ > 30) { retry_.stop(); return; }
    pending_ = true;
    const auto generation = generation_;
    auto *lookup = new QDBusPendingCallWatcher(bus_->asyncCall(daemonCall("GetNameOwner", service_), 1000), this);
    connect(lookup, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *found) {
        const QDBusPendingReply<QString> owner = *found;
        found->deleteLater();
        if (!bus_ || generation != generation_) return;
        if (owner.isError() || !owner.value().startsWith(QLatin1Char(':'))) { pending_ = false; return; }
        const QString candidate = owner.value();
        auto *identity = new QDBusPendingCallWatcher(bus_->asyncCall(daemonCall("GetConnectionUnixUser", candidate), 1000), this);
        connect(identity, &QDBusPendingCallWatcher::finished, this, [this, generation, candidate](QDBusPendingCallWatcher *checked) {
            const QDBusPendingReply<uint> uid = *checked;
            checked->deleteLater();
            if (!bus_ || generation != generation_) return;
            if (uid.isError()) { pending_ = false; return; }
            if (uid.value() != static_cast<uint>(geteuid())) { pending_ = false; retry_.stop(); return; }
            owner_ = candidate;
            auto request = QDBusMessage::createMethodCall(owner_, path_, QString::fromLatin1(Service), QStringLiteral("AttachSessionWithDisplay"));
            request.setArguments({display_}); request.setAutoStartService(false);
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
        });
    });
}
void PortalSessionLifetime::stop() noexcept {
    ++generation_; pending_ = false; retry_.stop(); watcher_.reset();
    if (bus_) { QDBusConnection::disconnectFromBus(connectionName_); bus_.reset(); }
    display_.clear(); owner_.clear(); attempts_ = 0;
}
}
