// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_ui.h>
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/session_binding.h>
#include <memory>
namespace QindaQt::Services::Portal {
// Owns bounded capture children and temporary results; no resident GUI/PipeWire
// connection. All borrowed objects and bus outlive this same-thread port.
// Helper path is trusted composition input; two freshly admitted ordinary FDs
// carry UI and restricted capture protocol, never ambient display authority.
class ProcessCapture final : public CaptureUI {
    Q_OBJECT
public:
    ProcessCapture(PortalSessionBinding &, AccessConsent &, RequestRegistry &,
        QDBusConnection, QString helper, QString privateRuntime);
    ~ProcessCapture() override;
    bool admitted() const override;
    void request(RequestToken, const CaptureRequest &) override;
    void cancel(RequestToken) override;
    void stop(const QString &) override;
    void revoke() override;
private:
    class Private; std::unique_ptr<Private> d;
};
}
