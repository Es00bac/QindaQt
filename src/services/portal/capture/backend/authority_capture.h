// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_ui.h>
#include <QDBusConnection>
#include <memory>
namespace QindaQt::Services::Portal {
// Capture-only broker port. Owns inherited compositor control, bounded jobs,
// pipe endpoints and private result files; borrows registry for its lifetime.
// Same Qt thread. Never launches children or opens ambient display sockets.
// A missing/malformed protected channel makes the port permanently unavailable.
// Successful Screenshot retains its helper/URI <=5min; stream retains Session.
class AuthorityCapture final : public CaptureUI {
    Q_OBJECT
public:
    AuthorityCapture(RequestRegistry &, QDBusConnection, QString runtimeDirectory,
                     int ownedControlFd, QObject *parent = nullptr);
    ~AuthorityCapture() override;
    // Startup transport availability; pixel admission may still be Unknown.
    bool available() const;
    bool initialized() const;
    // Call once, after publishing the fixed backend name and only while the
    // actual native observer is initialized. Ready authenticates that name.
    // Failure is terminal; no restart or transfer to a new native owner.
    bool start();
    bool admitted() const override;
    void request(RequestToken, const CaptureRequest &) override;
    void cancel(RequestToken) override;
    void stop(const QString &session) override;
    void revoke() override;
Q_SIGNALS:
    void initializedChanged(bool initialized);
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
