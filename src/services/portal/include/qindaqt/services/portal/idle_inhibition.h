// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/request_registry.h>
#include <qindaqt/services/power_client/power_transport.h>
#include <functional>
#include <memory>
namespace QindaQt::Services::Portal {
class IdleInhibition : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void acquire(RequestToken, const QString &app, const QString &reason) = 0;
    virtual void cancel(RequestToken) = 0;
Q_SIGNALS:
    void acquired(RequestToken token, bool success);
    void unavailable();
};
// Native portal Idle (8) covers every native idle stage: automatic lock,
// display off AND idle suspend. Partial support must fail, never silently
// inhibit just a subset. Logout/user-switch/explicit suspend are separate,
// unsupported contracts. No fake session monitor or end-session state exists.
// Owns activation of the borrowed transport; that same-thread transport and
// readonly/non-reentrant session admission callback outlive this object.
// No acquisition replay after owner loss. Cancel releases an acquired handle;
// late acquisition after cancellation is released using its original owner.
class PowerIdleInhibition final : public IdleInhibition {
    Q_OBJECT
public:
    PowerIdleInhibition(QindaQt::Power::PowerTransport &, std::function<bool()> admission,
                        QObject *parent = nullptr);
    ~PowerIdleInhibition() override;
    void acquire(RequestToken, const QString &, const QString &) override;
    void cancel(RequestToken) override;
    void revoke();
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal
