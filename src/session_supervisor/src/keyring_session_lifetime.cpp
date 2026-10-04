// SPDX-License-Identifier: LGPL-3.0-or-later
#include "keyring_session_lifetime.h"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
namespace {
constexpr auto Service = "org.qindaqt.Keyring1";
constexpr auto Path = "/org/freedesktop/secrets";
constexpr int MaximumAttempts = 30;
}
KeyringSessionLifetime::KeyringSessionLifetime(QObject *parent) : QObject(parent) {
    admission_.setInterval(100);
    connect(&admission_, &QTimer::timeout, this, &KeyringSessionLifetime::attach);
}
KeyringSessionLifetime::~KeyringSessionLifetime() { stop(); }
void KeyringSessionLifetime::start(const QString &program) {
    if (program.isEmpty() || bus_) return;
    connectionName_ = "qindaqt-keyring-session-" + QUuid::createUuid().toString(QUuid::Id128);
    bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName_));
    if (!bus_->isConnected()) { stop(); return; }
    bus_->interface()->setTimeout(250);
    display_ = qEnvironmentVariable("WAYLAND_DISPLAY");
    watcher_ = std::make_unique<QDBusServiceWatcher>(Service, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
    connect(watcher_.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &newOwner) { observeOwner(newOwner); });
    child_.start(program);
    admission_.start();
    attach();
}
std::optional<QString> KeyringSessionLifetime::currentOwner() const {
    if (!bus_ || !bus_->isConnected()) return std::nullopt;
    const QDBusReply<QString> owner = bus_->interface()->serviceOwner(Service);
    if (owner.isValid() && owner.value().startsWith(QLatin1Char(':'))) return owner.value();
    if (!owner.isValid() && owner.error().name() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")) return QString{};
    // AGENT-GUARD: transport uncertainty is not confirmed name absence. A
    // one-time lookup timeout must not consume the only replacement event.
    return std::nullopt;
}
void KeyringSessionLifetime::selectOwner(const QString &owner) {
    ++generation_;
    delete pending_.data();
    pending_.clear();
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
    const auto owner = currentOwner();
    if (!owner) {
        if (advertisedOwner.startsWith(QLatin1Char(':')) && advertisedOwner != owner_) {
            // An arrival hint gets a fresh retry budget, never admission. Each
            // attempt still requires actual owner and same-UID metadata.
            selectOwner(advertisedOwner);
        } else {
            ++generation_;
            delete pending_.data();
            pending_.clear();
            retryUnavailable();
        }
        return;
    }
    if (*owner == owner_) return;
    selectOwner(*owner);
    if (!owner_.isEmpty()) attach();
}
void KeyringSessionLifetime::attach() {
    if (!bus_ || pending_) return;
    if (!bus_->isConnected()) { admission_.stop(); return; }
    const auto owner = currentOwner();
    if (!owner) { ++attempts_; retryUnavailable(); return; }
    if (*owner != owner_) selectOwner(*owner);
    if (++attempts_ > MaximumAttempts) { admission_.stop(); return; }
    if (owner_.isEmpty()) return;
    const QDBusReply<uint> uid = bus_->interface()->serviceUid(owner_);
    if (!uid.isValid()) { retryUnavailable(); return; }
    if (uid.value() != static_cast<uint>(geteuid())) { admission_.stop(); return; }
    auto request = QDBusMessage::createMethodCall(owner_, Path, Service,
        display_.isEmpty() ? "AttachSession" : "AttachSessionWithDisplay");
    if (!display_.isEmpty()) request.setArguments({display_});
    request.setAutoStartService(false);
    const auto selectedOwner = owner_;
    const auto generation = generation_;
    pending_ = new QDBusPendingCallWatcher(bus_->asyncCall(request, 500), this);
    connect(pending_.data(), &QDBusPendingCallWatcher::finished, this,
        [this, selectedOwner, generation](QDBusPendingCallWatcher *done) {
            const QDBusPendingReply<bool> reply = *done;
            done->deleteLater();
            if (!bus_ || generation != generation_) return;
            pending_.clear();
            const auto current = currentOwner();
            if (!current) { retryUnavailable(); return; }
            if (*current != selectedOwner) { observeOwner(*current); return; }
            const QDBusReply<uint> replyUid = bus_->interface()->serviceUid(selectedOwner);
            if (!replyUid.isValid()) { retryUnavailable(); return; }
            if (replyUid.value() != static_cast<uint>(geteuid())) { admission_.stop(); return; }
            const auto finalOwner = currentOwner();
            if (!finalOwner) { retryUnavailable(); return; }
            if (*finalOwner != selectedOwner) { observeOwner(*finalOwner); return; }
            if (!reply.isError() && reply.value()) {
                attachedOwner_ = selectedOwner;
                admission_.stop();
            } else if (display_.isEmpty()) admission_.stop();
        });
}
void KeyringSessionLifetime::stop() noexcept {
    ++generation_;
    admission_.stop();
    watcher_.reset();
    delete pending_.data();
    pending_.clear();
    const auto admittedOwner = attachedOwner_;
    attachedOwner_.clear();
    owner_.clear();
    child_.stop();
    if (bus_) {
        // AGENT-GUARD: service-name Shutdown could kill a replacement that has
        // never accepted this session. Only the still-current admitted unique
        // owner may receive it; stop retires the watcher before any child wait.
        if (!admittedOwner.isEmpty() && currentOwner() == admittedOwner) {
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
