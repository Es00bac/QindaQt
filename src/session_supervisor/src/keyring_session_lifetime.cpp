// SPDX-License-Identifier: LGPL-3.0-or-later
#include "keyring_session_lifetime.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
namespace {
constexpr auto Service = "org.qindaqt.Keyring1";
constexpr auto Path = "/org/freedesktop/secrets";
constexpr int MaximumAttempts = 30;
constexpr int LookupTimeoutMs = 1'000;
QDBusMessage daemonCall(const char *method, const QString &argument) {
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QString::fromLatin1(method));
    message << argument;
    return message;
}
}
KeyringSessionLifetime::KeyringSessionLifetime(QObject *parent) : QObject(parent) {
    // AGENT-NOTE (ADR-0348, ADR-0379): one tick per 100 ms while an owner is
    // unadmitted, at most thirty attempts per owner. Each attempt is a chain of
    // asynchronous calls, so the tick never blocks the supervisor.
    admission_.setInterval(100);
    connect(&admission_, &QTimer::timeout, this, &KeyringSessionLifetime::attach);
}
KeyringSessionLifetime::~KeyringSessionLifetime() { stop(); }
void KeyringSessionLifetime::start(const QString &program) {
    if (program.isEmpty() || bus_) return;
    connectionName_ = "qindaqt-keyring-session-" + QUuid::createUuid().toString(QUuid::Id128);
    bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName_));
    if (!bus_->isConnected()) { stop(); return; }
    display_ = qEnvironmentVariable("WAYLAND_DISPLAY");
    watcher_ = std::make_unique<QDBusServiceWatcher>(Service, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
    connect(watcher_.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &newOwner) { observeOwner(newOwner); });
    child_.start(program);
    admission_.start();
    attach();
}
quint64 KeyringSessionLifetime::startChain() { pending_ = true; return ++chain_; }
void KeyringSessionLifetime::cancelChain() { ++chain_; pending_ = false; }
void KeyringSessionLifetime::lookupOwner(Fence current, OwnerReply done) {
    auto *call = new QDBusPendingCallWatcher(
        bus_->asyncCall(daemonCall("GetNameOwner", QString::fromLatin1(Service)), LookupTimeoutMs), this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this, current = std::move(current), done = std::move(done)](QDBusPendingCallWatcher *finished) {
            const QDBusPendingReply<QString> owner = *finished;
            finished->deleteLater();
            if (!bus_ || !current()) return;
            if (!owner.isError() && owner.value().startsWith(QLatin1Char(':'))) { done(owner.value()); return; }
            // AGENT-GUARD: transport uncertainty is not confirmed name absence. A
            // one-time lookup timeout must not consume the only replacement event.
            if (owner.isError() && owner.error().name() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")) {
                done(QString{});
                return;
            }
            done(std::nullopt);
        });
}
void KeyringSessionLifetime::lookupUid(Fence current, const QString &owner, UidReply done) {
    auto *call = new QDBusPendingCallWatcher(
        bus_->asyncCall(daemonCall("GetConnectionUnixUser", owner), LookupTimeoutMs), this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this, current = std::move(current), done = std::move(done)](QDBusPendingCallWatcher *finished) {
            const QDBusPendingReply<uint> uid = *finished;
            finished->deleteLater();
            if (!bus_ || !current()) return;
            done(uid.isError() ? std::nullopt : std::optional<uint>(uid.value()));
        });
}
std::optional<QString> KeyringSessionLifetime::currentOwnerBlocking() const {
    if (!bus_ || !bus_->isConnected()) return std::nullopt;
    const QDBusMessage reply = bus_->call(daemonCall("GetNameOwner", QString::fromLatin1(Service)), QDBus::Block, 250);
    if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1) {
        const auto owner = reply.arguments().first().toString();
        if (owner.startsWith(QLatin1Char(':'))) return owner;
    }
    if (reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")) return QString{};
    return std::nullopt;
}
void KeyringSessionLifetime::selectOwner(const QString &owner) {
    owner_ = owner;
    attachedOwner_.clear();
    attempts_ = 0;
    if (owner_.isEmpty()) admission_.stop();
    else admission_.start();
}
void KeyringSessionLifetime::retryUnavailable() {
    attachedOwner_.clear();
    if (attempts_ < MaximumAttempts) admission_.start();
    else admission_.stop();
}
void KeyringSessionLifetime::observeOwner(const QString &advertisedOwner) {
    if (!bus_) return;
    // AGENT-GUARD: queued old loss/arrival signals can follow a newer lookup.
    // Query the actual current owner; never replay or revoke an identical pin.
    const auto observation = ++observations_;
    lookupOwner([this, observation] { return observation == observations_; },
        [this, advertisedOwner](const std::optional<QString> &owner) {
            if (!owner) {
                if (advertisedOwner.startsWith(QLatin1Char(':')) && advertisedOwner != owner_) {
                    // An arrival hint gets a fresh retry budget, never admission.
                    // Each attempt still requires actual owner and same-UID metadata.
                    cancelChain();
                    selectOwner(advertisedOwner);
                } else {
                    cancelChain();
                    retryUnavailable();
                }
                return;
            }
            observeCurrent(*owner);
        });
}
void KeyringSessionLifetime::observeCurrent(const QString &owner) {
    if (owner == owner_) return;
    // Replacement retires the in-flight attempt and its late replies.
    cancelChain();
    selectOwner(owner);
    if (!owner_.isEmpty()) attach();
}
void KeyringSessionLifetime::attach() {
    if (!bus_ || pending_) return;
    if (!bus_->isConnected()) { admission_.stop(); return; }
    const auto token = startChain();
    const Fence current = [this, token] { return token == chain_; };
    lookupOwner(current, [this, token, current](const std::optional<QString> &owner) {
        if (!owner) { ++attempts_; pending_ = false; retryUnavailable(); return; }
        if (*owner != owner_) selectOwner(*owner);
        if (++attempts_ > MaximumAttempts) { pending_ = false; admission_.stop(); return; }
        if (owner_.isEmpty()) { pending_ = false; return; }
        const auto selected = owner_;
        lookupUid(current, selected, [this, token, selected](const std::optional<uint> &uid) {
            if (!uid) { pending_ = false; retryUnavailable(); return; }
            if (*uid != static_cast<uint>(geteuid())) { pending_ = false; admission_.stop(); return; }
            sendAttach(token, selected);
        });
    });
}
void KeyringSessionLifetime::sendAttach(quint64 token, const QString &selected) {
    auto request = QDBusMessage::createMethodCall(selected, Path, Service,
        display_.isEmpty() ? "AttachSession" : "AttachSessionWithDisplay");
    if (!display_.isEmpty()) request.setArguments({display_});
    request.setAutoStartService(false);
    const Fence current = [this, token] { return token == chain_; };
    auto *call = new QDBusPendingCallWatcher(bus_->asyncCall(request, 500), this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this, current, selected](QDBusPendingCallWatcher *done) {
            const QDBusPendingReply<bool> reply = *done;
            done->deleteLater();
            if (!bus_ || !current()) return;
            const bool accepted = !reply.isError() && reply.value();
            // A reply is accepted only while its owner and UID are still current.
            lookupOwner(current, [this, current, selected, accepted](const std::optional<QString> &owner) {
                if (!owner) { pending_ = false; retryUnavailable(); return; }
                if (*owner != selected) { pending_ = false; observeCurrent(*owner); return; }
                lookupUid(current, selected, [this, selected, accepted](const std::optional<uint> &uid) {
                    pending_ = false;
                    if (!uid) { retryUnavailable(); return; }
                    if (*uid != static_cast<uint>(geteuid())) { admission_.stop(); return; }
                    if (accepted) {
                        attachedOwner_ = selected;
                        admission_.stop();
                    } else if (display_.isEmpty()) admission_.stop();
                });
            });
        });
}
void KeyringSessionLifetime::stop() noexcept {
    cancelChain();
    ++observations_;
    admission_.stop();
    watcher_.reset();
    const auto admittedOwner = attachedOwner_;
    attachedOwner_.clear();
    owner_.clear();
    child_.stop();
    if (bus_) {
        // AGENT-GUARD: service-name Shutdown could kill a replacement that has
        // never accepted this session. Only the still-current admitted unique
        // owner may receive it; stop retires the watcher before any child wait.
        if (!admittedOwner.isEmpty() && currentOwnerBlocking() == admittedOwner) {
            auto request = QDBusMessage::createMethodCall(admittedOwner, Path, Service, "Shutdown");
            request.setAutoStartService(false);
            bus_->call(request, QDBus::Block, 500);
        }
        QDBusConnection::disconnectFromBus(connectionName_);
        bus_.reset();
    }
    display_.clear();
    attempts_ = 0;
}
}
