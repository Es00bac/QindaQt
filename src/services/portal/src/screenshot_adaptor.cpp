// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/screenshot_adaptor.h>
namespace QindaQt::Services::Portal {
ScreenshotAdaptor::ScreenshotAdaptor(QObject &host, RequestRegistry &requests, CaptureUI &ui)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui) {
    registerCaptureWireTypes();
    connect(&ui, &CaptureUI::completed, this, [this](RequestToken token, RequestResponse response, const QVariantMap &results) {
        if (!m_pending.contains(token)) return;
        if (!m_ui.admitted() || !m_requests.live(token) || (response == RequestResponse::Success && !validCapturePublication(m_pending.value(token), results))) response = RequestResponse::Failed;
        m_requests.finish(token, response, results);
    });
    connect(&ui, &CaptureUI::authorityLost, this, [this] { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); });
}
ScreenshotAdaptor::~ScreenshotAdaptor() { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); }
quint32 ScreenshotAdaptor::begin(bool color, const QDBusObjectPath &handle, const QString &app, const QString &parent, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    // AGENT-CONTRACT: request lifetime bounds60s native selection plus30s
    // protected write/margin. The operation itself never extends its30s clock.
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse response) { m_pending.remove(*slot); if (response != RequestResponse::Success) m_ui.cancel(*slot); }, 100000);
    *slot = token; if (!token) return 2;
    auto request = screenshotRequest(app, parent, options, color);
    if (!request || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    request->caller = captureCaller(handle.path());
    if (request->caller.isEmpty()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_pending.insert(token, request->kind); m_ui.request(token, *request); return 2;
}
quint32 ScreenshotAdaptor::Screenshot(const QDBusObjectPath &h, const QString &a, const QString &p, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(false, h, a, p, o, c, r); }
quint32 ScreenshotAdaptor::PickColor(const QDBusObjectPath &h, const QString &a, const QString &p, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(true, h, a, p, o, c, r); }
}
