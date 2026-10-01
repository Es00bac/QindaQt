// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/app_chooser_adaptor.h>
namespace QindaQt::Services::Portal {
AppChooserAdaptor::AppChooserAdaptor(QObject &host, RequestRegistry &requests, ChooserUi &ui, Catalog catalog)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui), m_catalog(std::move(catalog)) {
    connect(&ui, &ChooserUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        const auto results = appChooserResults(*it, output);
        if (!m_ui.admitted() || !m_requests.live(token) || (response == RequestResponse::Success && !results)) response = RequestResponse::Failed;
        m_requests.finish(token, response, results.value_or(QVariantMap{}));
    });
    connect(&ui, &ChooserUi::authorityLost, this, [this] { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); });
}
AppChooserAdaptor::~AppChooserAdaptor() { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); }
quint32 AppChooserAdaptor::ChooseApplication(const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QStringList &choices, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0); const auto path = handle.path();
    const auto token = m_requests.begin(call, path, app, [this, slot, path](RequestResponse) {
        m_handles.remove(path); m_pending.remove(*slot); m_ui.cancel(*slot);
    }); *slot = token; if (!token) return 2;
    const auto request = m_catalog ? appChooserRequest(app, parent, choices, options, m_catalog()) : std::nullopt;
    if (!request || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_pending.insert(token, *request); m_handles.insert(path, token); m_ui.chooseApplication(token, *request); return 2;
}
void AppChooserAdaptor::UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices, const QDBusMessage &call) {
    const auto token = m_handles.value(handle.path());
    // Updates belong to the exact live frontend/request; they cannot create or
    // revive a chooser, replace its app identity, or mutate another request.
    if (!m_requests.authenticated(call) || !token || !m_requests.live(token) || !m_ui.admitted() || !m_catalog) return;
    const auto candidates = applicationCandidates(choices, m_catalog());
    if (!candidates) { m_requests.retire(token, RequestResponse::Failed); return; }
    m_pending[token].candidates = *candidates; m_ui.updateApplications(token, *candidates);
}
} // namespace QindaQt::Services::Portal
