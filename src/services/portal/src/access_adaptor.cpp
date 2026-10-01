// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/access_adaptor.h>
namespace QindaQt::Services::Portal {
AccessAdaptor::AccessAdaptor(QObject &host, RequestRegistry &requests, AccessConsent &consent)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_consent(consent) {
    registerAccessTypes();
    connect(&consent, &AccessConsent::completed, this,
        [this](RequestToken token, RequestResponse response, const ChoiceValues &choices) {
            const auto it = m_questions.constFind(token);
            if (it == m_questions.cend()) return;
            if (!m_consent.admitted() || !m_requests.live(token)
                || (response == RequestResponse::Success && !validChoiceValues(*it, choices)))
                response = RequestResponse::Failed;
            const QVariantMap result{{QStringLiteral("choices"), QVariant::fromValue(choices)}};
            m_requests.finish(token, response, result);
        });
    connect(&consent, &AccessConsent::authorityLost, this, [this] {
        const auto tokens = m_questions.keys();
        for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed);
    });
}
AccessAdaptor::~AccessAdaptor() {
    const auto tokens = m_questions.keys();
    for (const auto token : tokens) m_requests.retire(token, RequestResponse::Failed);
}
quint32 AccessAdaptor::AccessDialog(const QDBusObjectPath &handle, const QString &app,
    const QString &parent, const QString &title, const QString &subtitle, const QString &body,
    const QVariantMap &options, const QDBusMessage &call, QVariantMap &results) {
    results.clear();
    const auto question = accessQuestion(app, parent, title, subtitle, body, options);
    const auto tokenSlot = std::make_shared<RequestToken>(0);
    const RequestToken token = m_requests.begin(call, handle.path(), app,
        [this, tokenSlot](RequestResponse) {
            m_questions.remove(*tokenSlot); m_consent.cancel(*tokenSlot);
        });
    *tokenSlot = token;
    if (!token) return 2;
    if (!question || !m_consent.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    m_questions.insert(token, *question);
    m_consent.ask(token, *question);
    return 2; // Real reply is delayed; this return value is never publication.
}
} // namespace QindaQt::Services::Portal
