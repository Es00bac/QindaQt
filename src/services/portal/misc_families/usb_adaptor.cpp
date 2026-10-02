// SPDX-License-Identifier: LGPL-3.0-or-later
#include "usb_adaptor.h"
#include <QJsonArray>
namespace QindaQt::Services::Portal {
UsbAdaptor::UsbAdaptor(QObject &host, RequestRegistry &requests, MiscUi &ui)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui) {
    registerMiscTypes();
    connect(&ui, &MiscUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        if (!m_ui.admitted() || !m_requests.live(token)) response = RequestResponse::Failed;
        auto selected = usbResults(*it, output); QVariantMap results;
        if (response == RequestResponse::Success && !selected) response = RequestResponse::Failed;
        if (response == RequestResponse::Success) results.insert("devices", QVariant::fromValue(*selected));
        m_requests.finish(token, response, response == RequestResponse::Success ? results : QVariantMap{});
    });
    connect(&ui, &MiscUi::authorityLost, this, [this] { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); });
}
UsbAdaptor::~UsbAdaptor() { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); }
quint32 UsbAdaptor::AcquireDevices(const QDBusObjectPath &handle, const QString &parent, const QString &app,
    const UsbDevices &devices, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) { m_pending.remove(*slot); m_ui.cancel(*slot); });
    *slot = token; if (!token) return 2;
    auto frame = miscFrame("usb", app, parent, "Allow USB Devices", options); const auto offered = usbDevicesFrame(devices);
    if (!frame || !offered || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    frame->insert("devices", *offered); m_pending.insert(token, devices); m_ui.present(token, *frame); return 2;
}
}
