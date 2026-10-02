// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launcher_adaptor.h"
#include <QJsonArray>
namespace QindaQt::Services::Portal {
LauncherAdaptor::LauncherAdaptor(QObject &host, RequestRegistry &requests, MiscUi &ui)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui) {
    registerMiscTypes();
    connect(&ui, &MiscUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        if (!m_ui.admitted() || !m_requests.live(token)) response = RequestResponse::Failed;
        auto result = launcherResults(*it, output); QVariantMap results;
        if (response == RequestResponse::Success && !result) response = RequestResponse::Failed;
        if (response == RequestResponse::Success) results = *result;
        m_requests.finish(token, response, response == RequestResponse::Success ? results : QVariantMap{});
    });
    connect(&ui, &MiscUi::authorityLost, this, [this] { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); });
}
LauncherAdaptor::~LauncherAdaptor() { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); }
quint32 LauncherAdaptor::PrepareInstall(const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QString &name, const QDBusVariant &icon, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) { m_pending.remove(*slot); m_ui.cancel(*slot); });
    *slot = token; if (!token) return 2;
    const auto frame = launcherFrame(app, parent, name, icon, options);
    if (!frame || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_pending.insert(token, *frame); m_ui.present(token, *frame); return 2;
}
quint32 LauncherAdaptor::RequestInstallToken(const QString &app, const QVariantMap &options, const QDBusMessage &call) {
    // The standard frontend issues/consumes the token and installs the desktop
    // entry. The backend only permits this exact upstream software-center list.
    return m_requests.authenticated(call) && m_ui.admitted() && options.size() <= 32 && noninteractiveLauncherAllowed(app) ? 0U : 2U;
}
}
