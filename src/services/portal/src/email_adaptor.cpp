// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/email_adaptor.h>
#include <qindaqt/services/portal/email_policy.h>
namespace QindaQt::Services::Portal {
using QindaQt::Services::ApplicationUri::UriOpenResult;
EmailAdaptor::EmailAdaptor(QObject &host, RequestRegistry &requests,
    QindaQt::Services::ApplicationUri::ApplicationUriOpener &opener, std::function<bool()> admission)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_opener(opener), m_admission(std::move(admission)) {
    connect(&opener, &QindaQt::Services::ApplicationUri::ApplicationUriOpener::completed, this,
        [this](quint64 token, UriOpenResult result) {
            if (!m_tokens.contains(token)) return;
            RequestResponse response = RequestResponse::Failed;
            if (result == UriOpenResult::Cancelled) response = RequestResponse::Cancelled;
            else if (result == UriOpenResult::Started && m_admission && m_admission() && m_requests.live(token)) response = RequestResponse::Success;
            m_requests.finish(token, response);
        });
}
EmailAdaptor::~EmailAdaptor() {
    const auto tokens = m_tokens.values(); for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed);
}
quint32 EmailAdaptor::ComposeEmail(const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) {
        m_tokens.remove(*slot); m_opener.cancel(*slot);
    }, 10000); *slot = token;
    if (!token) return 2;
    const auto draft = emailDraft(parent, options);
    if (!draft || !m_admission || !m_admission()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_tokens.insert(token); m_opener.open(token, draft->uri, draft->activationToken); return 2;
}
} // namespace QindaQt::Services::Portal
