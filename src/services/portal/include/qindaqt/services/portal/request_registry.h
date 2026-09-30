// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <functional>
#include <memory>

namespace QindaQt::Services::Portal {
using RequestToken = quint64;
enum class RequestResponse : quint32 { Success = 0, Cancelled = 1, Failed = 2 };
// Owns bounded standard Request objects and delayed method replies, never UI
// or service policy. Borrowed callbacks run synchronously on this QObject's
// thread after removal; they may finish other requests but must not delete the
// registry. Callers must outlive it or retire their tokens before destruction.
// App identity is frontend-supplied, case sensitive, and may be empty for host
// callers. Only the current same-UID portal frontend can begin/close requests.
class RequestRegistry final : public QObject {
    Q_OBJECT
public:
    using Retirement = std::function<void(RequestResponse)>;
    explicit RequestRegistry(QDBusConnection bus, QObject *parent = nullptr);
    ~RequestRegistry() override;
    // Returns zero and sends a named error on rejection. A successful begin
    // marks the call delayed and publishes its Close object before side effects.
    RequestToken begin(const QDBusMessage &call, const QString &path,
                       const QString &appId, Retirement retire,
                       int deadlineMs = 60000, bool voidMethod = false);
    bool live(RequestToken token) const;
    bool authenticated(const QDBusMessage &call) const;
    QString frontendOwner() const;
    // Complete response-bearing methods; removes the Close object first.
    void finish(RequestToken token, RequestResponse response,
                const QVariantMap &results = {});
    // Inhibit has a void backend method, followed by a live Request lifetime.
    // Acknowledge only after the actual native lease exists. Its acquisition
    // deadline ends here; Close/owner loss still retire the borrowed lease.
    void acknowledgeHeld(RequestToken token);
    void failVoid(RequestToken token, const QString &errorName);
    void retire(RequestToken token, RequestResponse reason);
    void retireAll(RequestResponse reason = RequestResponse::Failed);
private Q_SLOTS:
    void frontendChanged(const QString &name, const QString &oldOwner,
                         const QString &newOwner);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal
