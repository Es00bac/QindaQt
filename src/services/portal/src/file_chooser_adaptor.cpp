// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/file_chooser_adaptor.h>
namespace QindaQt::Services::Portal {
FileChooserAdaptor::FileChooserAdaptor(QObject &host, RequestRegistry &requests, ChooserUi &ui)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui) {
    registerChooserTypes();
    connect(&ui, &ChooserUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        auto results = fileChooserResults(*it, output);
        if (!m_ui.admitted() || !m_requests.live(token) || (response == RequestResponse::Success && !results)) response = RequestResponse::Failed;
        m_requests.finish(token, response, results.value_or(QVariantMap{}));
    });
    connect(&ui, &ChooserUi::authorityLost, this, [this] { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); });
}
FileChooserAdaptor::~FileChooserAdaptor() { const auto tokens = m_pending.keys(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed); }
quint32 FileChooserAdaptor::begin(FileChooserMode mode, const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QString &title, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) { m_pending.remove(*slot); m_ui.cancel(*slot); });
    *slot = token; if (!token) return 2;
    const auto request = fileChooserRequest(mode, app, parent, title, options);
    if (!request || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_pending.insert(token, *request); m_ui.openFile(token, *request); return 2;
}
quint32 FileChooserAdaptor::OpenFile(const QDBusObjectPath &h, const QString &a, const QString &p, const QString &t, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(FileChooserMode::Open, h, a, p, t, o, c, r); }
quint32 FileChooserAdaptor::SaveFile(const QDBusObjectPath &h, const QString &a, const QString &p, const QString &t, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(FileChooserMode::Save, h, a, p, t, o, c, r); }
quint32 FileChooserAdaptor::SaveFiles(const QDBusObjectPath &h, const QString &a, const QString &p, const QString &t, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(FileChooserMode::SaveMany, h, a, p, t, o, c, r); }
} // namespace QindaQt::Services::Portal
