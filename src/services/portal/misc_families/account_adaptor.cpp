// SPDX-License-Identifier: LGPL-3.0-or-later
#include "account_adaptor.h"
#include <QJsonArray>
namespace QindaQt::Services::Portal {
AccountAdaptor::AccountAdaptor(QObject &host, RequestRegistry &requests, MiscUi &ui, Provider provider)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui), m_provider(std::move(provider)) {
    registerMiscTypes();
    connect(&ui, &MiscUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        if (!m_ui.admitted() || !m_requests.live(token)) response = RequestResponse::Failed;
        QVariantMap results;
        if (response == RequestResponse::Success) {
            if (output.size() != 3) response = RequestResponse::Failed;
            for (const auto *key : {"id", "name", "image"}) if (!output.value(key).isBool()) response = RequestResponse::Failed;
            if (response == RequestResponse::Success) {
                results.insert("id", output.value("id").toBool() ? it->id : QString{});
                results.insert("name", output.value("name").toBool() ? it->name : QString{});
                results.insert("image", output.value("image").toBool() ? it->image : QStringLiteral("file://"));
            }
        }
        m_requests.finish(token, response, response == RequestResponse::Success ? results : QVariantMap{});
    });
    connect(&ui, &MiscUi::authorityLost, this, [this] { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); });
}
AccountAdaptor::~AccountAdaptor() { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); }
quint32 AccountAdaptor::GetUserInformation(const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) { m_pending.remove(*slot); m_ui.cancel(*slot); });
    *slot = token; if (!token) return 2;
    auto frame = miscFrame("account", app, parent, "Share Account Information", options);
    const auto reason = options.value("reason");
    if (!frame || !m_ui.admitted() || (reason.isValid() && (reason.metaType() != QMetaType::fromType<QString>() || !boundedText(reason.toString())))) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    const auto info = m_provider();
    if (info.id.isEmpty() || !boundedText(info.id, 256) || !boundedText(info.name, 1024) || !boundedText(info.image, 4096)) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    frame->insert("reason", reason.toString()); frame->insert("id", info.id); frame->insert("name", info.name); frame->insert("image", info.image);
    m_pending.insert(token, info); m_ui.present(token, *frame); return 2;
}
}
