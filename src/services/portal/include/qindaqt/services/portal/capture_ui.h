// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_types.h>
#include <qindaqt/services/portal/request_registry.h>
namespace QindaQt::Services::Portal {
// Same-thread public native UI/capture port. Borrowed registry/binding/admission
// dependencies outlive it. One completion per pending token; cancellation emits
// no completion. Successful stream completion retains its session until stop or
// closed. Screenshot files remain owner-only temporary results for at most5min,
// revoked on authority loss/destruction. Already copied bytes cannot be recalled.
class CaptureUI : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool admitted() const = 0;
    virtual void request(RequestToken, const CaptureRequest &) = 0;
    virtual void cancel(RequestToken) = 0;
    virtual void stop(const QString &session) = 0;
    virtual void revoke() = 0;
Q_SIGNALS:
    void completed(RequestToken, RequestResponse, const QVariantMap &);
    void closed(const QString &session);
    void authorityLost();
};
}
