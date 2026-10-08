// SPDX-License-Identifier: LGPL-3.0-or-later
// The staged header is explicit so a withheld SDK cannot silently fall back to
// an ambient installed QindaQt header with the same new constructor API.
#ifndef QINDAQT_STAGED_CLIPBOARD_HOST_HEADER
#error "The installed-only fixture requires its injected staged Host header"
#endif
#include QINDAQT_STAGED_CLIPBOARD_HOST_HEADER
#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>
#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>
#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>
#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLatin1String>

#include <memory>

namespace {
using namespace QindaQt::Services;
class SyntheticAdapter final : public ClipboardWayland::ClipboardWaylandAdapter {
public:
    void setObserver(ClipboardWayland::CaptureObserver *) override {}
    ClipboardWayland::StartStatus start() override { return ClipboardWayland::StartStatus::Started; }
    void stop() override { enabled = false; }
    void setCaptureEnabled(bool value) override { enabled = value; }
    bool isAvailable() const noexcept override { return true; }
    qint64 peerProcessId() const noexcept override { return 42; }
    bool publishSelection(ClipboardWayland::SelectionKind,
                          const ClipboardModel::ClipboardValue &) override { return true; }
    bool enabled = false;
};
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    using namespace QindaQt::Services::Clipboard;
    registerDBusTypes();
    if (kSchemaVersion != 1
        || QLatin1String(kServiceName) != QLatin1String("org.qindaqt.Clipboard1")) return 1;
    OperationRequest request{.kind = OperationKind::Clear, .requestId = 1,
        .expectedEpoch = 2, .expectedGeneration = 1, .expectedRevision = 0,
        .entry = {}, .clearAll = true};
    if (!validateOperationRequest(request).accepted) return 2;

    // Actual new public constructor symbols must link from the installed
    // archive. No source headers, host bus/display or real clipboard is used.
    SyntheticAdapter adapter;
    ClipboardHost host(&adapter, 2, PrivacyAdmission{});
    host.setHistoryOptIn(true);
    host.setUnlocked(true);
    if (host.snapshot().privacyAllowed || adapter.enabled) return 3;
    ResidentClipboardService service(std::make_unique<SyntheticAdapter>(),
        QDBusConnection(QStringLiteral("invalid")), {}, 3, [] { return false; });
    service.host()->setHistoryOptIn(true);
    service.host()->setUnlocked(true);
    if (service.host()->snapshot().privacyAllowed
        || service.start() != ServiceStartStatus::InvalidConnection) return 4;
    return 0;
}
