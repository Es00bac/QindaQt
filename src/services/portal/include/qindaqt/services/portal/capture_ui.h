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
// AGENT-CONTRACT: xdg-desktop-portal 1.20 selects the ScreenCast and
// RemoteDesktop backends independently and sends ScreenCast.SelectSources for
// a RemoteDesktop session to the ScreenCast backend with that session handle
// (frontend screen-cast.c). The backend owning such a session implements this
// same-thread port; ScreenCastAdaptor validates the standard options first and
// forwards only the frozen monitor selection. The owner starts and stops the
// streams through its CaptureUI and must outlive the ScreenCastAdaptor.
class ScreenCastSourceDelegate {
public:
    virtual ~ScreenCastSourceDelegate() = default;
    virtual bool ownsSession(const QString &session) const = 0;
    // False refuses the selection: wrong actor/handle/state or lost authority.
    virtual bool selectSources(const QDBusMessage &call, const QString &request, const QString &session,
                               const QString &app, bool multiple, quint32 cursorMode) = 0;
};
}
