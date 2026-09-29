// SPDX-License-Identifier: LGPL-3.0-or-later
#include "keyring_session_lifetime.h"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::SessionSupervisor {
namespace { constexpr auto Service = "org.qindaqt.Keyring1"; constexpr auto Path = "/org/freedesktop/secrets"; }
KeyringSessionLifetime::KeyringSessionLifetime(QObject *parent) : QObject(parent) {
    admission_.setInterval(100);
    connect(&admission_,&QTimer::timeout,this,[this] { attach(); });
}
KeyringSessionLifetime::~KeyringSessionLifetime() { stop(); }
void KeyringSessionLifetime::start(const QString &program) {
    if (program.isEmpty() || bus_) return;
    connectionName_ = "qindaqt-keyring-session-" + QUuid::createUuid().toString(QUuid::Id128);
    bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus,connectionName_));
    if (!bus_->isConnected()) { stop(); return; }
    child_.start(program);
    attempts_ = 0;
    admission_.start();
}
void KeyringSessionLifetime::attach() {
    if (!bus_ || ++attempts_ > 30) { admission_.stop(); return; }
    const auto owner = bus_->interface()->serviceOwner(Service);
    if (!owner.isValid() || owner.value().isEmpty()) return;
    const auto uid = bus_->interface()->serviceUid(owner.value());
    if (!uid.isValid() || uid.value() != geteuid()) { admission_.stop(); return; }
    auto request = QDBusMessage::createMethodCall(owner.value(),Path,Service,"AttachSession");
    request.setAutoStartService(false);
    QDBusReply<bool> reply = bus_->call(request,QDBus::Block,500);
    attached_ = reply.isValid() && reply.value();
    admission_.stop();
}
void KeyringSessionLifetime::stop() noexcept {
    admission_.stop();
    child_.stop();
    if (bus_) {
        if (attached_ && bus_->isConnected()) {
            auto request = QDBusMessage::createMethodCall(Service,Path,Service,"Shutdown");
            request.setAutoStartService(false);
            bus_->call(request,QDBus::Block,500);
        }
        QDBusConnection::disconnectFromBus(connectionName_);
        bus_.reset();
    }
    attached_ = false;
}
}
