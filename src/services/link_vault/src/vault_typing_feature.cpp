// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/session.h>
#include <qindaqt/services/link_vault/vault_typing_feature.h>

#include <QtCore/QRandomGenerator>

namespace QindaQt::Link {
namespace {

QString newToken()
{
    quint32 raw[4];
    QRandomGenerator::system()->fillRange(raw);
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(raw), sizeof raw).toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool hasVaultCap(const Session &session)
{
    return session.peer().caps.contains(QStringLiteral("vault"));
}

} // namespace

VaultTypingFeature::VaultTypingFeature(PortalInputSession &portal, InputGate &gate, FocusSource &focus,
                                       QObject *parent)
    : QObject(parent), m_portal(portal), m_gate(gate), m_focus(focus), m_held(portal), m_text(portal)
{
    m_delay.setSingleShot(true);
    connect(&m_delay, &QTimer::timeout, this, &VaultTypingFeature::advance);
    // AGENT-GUARD: checked by the typer before every batch, so a focus change is noticed within one
    // batch even when no signal fired. The reason is remembered for finished().
    m_text.setBatchGuard([this] {
        m_guardProblem = problem();
        return m_guardProblem.isEmpty();
    });
    connect(&m_text, &TextTyper::finished, this, [this](const QString &token, bool ok) {
        if (!m_job || token != m_job->textToken) return; // a job aborted from elsewhere already answered
        m_job->textToken.clear();
        if (ok) {
            advance();
        } else {
            abortWith(m_guardProblem.isEmpty() ? QStringLiteral("internal") : m_guardProblem);
        }
    });
    connect(&m_gate, &InputGate::changed, this, [this] {
        if (!m_job) return;
        const QString p = problem();
        if (!p.isEmpty()) abortWith(p);
    });
    connect(&m_focus, &FocusSource::changed, this, [this] {
        if (!m_job || m_job->waitingForPortal) return;
        const QString p = problem();
        if (!p.isEmpty()) abortWith(p);
    });
    connect(&m_portal, &PortalInputSession::activated, this, [this] {
        if (m_job && m_job->waitingForPortal) {
            m_job->waitingForPortal = false;
            advance();
        }
    });
    connect(&m_portal, &PortalInputSession::failed, this,
            [this](PortalInputSession::Failure f, const QString &) { onPortalFailed(f); });
    connect(&m_portal, &PortalInputSession::revoked, this, [this] {
        if (m_job) abortWith(QStringLiteral("internal"), QStringLiteral("remote input was revoked"));
    });
}

VaultTypingFeature::~VaultTypingFeature() { shutdown(); }

void VaultTypingFeature::shutdown()
{
    if (m_job) {
        m_job->session = nullptr;
        finish(QString());
    }
}

void VaultTypingFeature::sendError(Session &session, qint64 id, const QString &code, const QString &message)
{
    if (id != 0) session.send(makeError(code, message, id));
}

void VaultTypingFeature::handle(Session &session, const Envelope &e)
{
    if (e.t == QLatin1String("desktop.focus")) return focusRequest(session, e);
    if (e.t == QLatin1String("vault.type")) return typeRequest(session, e);
    sendError(session, e.id, QStringLiteral("unknown-type"), QStringLiteral("no handler"));
}

void VaultTypingFeature::sessionClosed(const QString &deviceId)
{
    m_tokens.remove(deviceId);
    if (m_job && m_job->device == deviceId) {
        m_job->session = nullptr; // never touch the dying session again
        abortWith(QStringLiteral("internal"));
    }
}

QString VaultTypingFeature::problem() const
{
    if (!m_gate.open()) {
        return m_gate.reason() == InputGate::Reason::Locked ? QStringLiteral("locked") : QStringLiteral("unverified");
    }
    const std::optional<FocusFact> focus = m_focus.current();
    if (!focus) return QStringLiteral("unverified");
    if (!m_job) return {};
    if (focus->uuid != m_job->uuid || focus->revision != m_job->revision) return QStringLiteral("target-changed");
    if (!m_job->waitingForPortal && !m_portal.active()) return QStringLiteral("internal");
    return {};
}

void VaultTypingFeature::focusRequest(Session &session, const Envelope &e)
{
    if (e.id == 0) return; // a request; nothing to answer
    if (!hasVaultCap(session) && !session.peer().caps.contains(QStringLiteral("input"))) {
        return sendError(session, e.id, QStringLiteral("denied"), QStringLiteral("capability not advertised"));
    }
    if (!m_gate.open()) {
        return sendError(session, e.id,
                         m_gate.reason() == InputGate::Reason::Locked ? QStringLiteral("locked")
                                                                      : QStringLiteral("unverified"));
    }
    const std::optional<FocusFact> focus = m_focus.current();
    if (!focus) return sendError(session, e.id, QStringLiteral("unverified"), QStringLiteral("focus unknown"));
    Token t;
    t.value = newToken();
    t.uuid = focus->uuid;
    t.revision = focus->revision;
    t.age.start();
    m_tokens.insert(session.deviceId(), t);
    Envelope r;
    r.t = QStringLiteral("desktop.focus");
    r.re = e.id;
    r.b = QJsonObject{{QStringLiteral("token"), t.value},
                      {QStringLiteral("app"), focus->appId},
                      {QStringLiteral("title"), focus->title}};
    session.send(r);
}

void VaultTypingFeature::typeRequest(Session &session, const Envelope &e)
{
    if (e.id == 0) return;
    if (!hasVaultCap(session)) {
        return sendError(session, e.id, QStringLiteral("denied"), QStringLiteral("capability not advertised"));
    }
    QString tokenValue;
    std::optional<SecretPlan> plan = SecretPlan::parse(e.b, &tokenValue);
    if (!plan) return sendError(session, e.id, QStringLiteral("bad-request"), QStringLiteral("invalid vault.type"));
    if (m_job) return sendError(session, e.id, QStringLiteral("busy"), QStringLiteral("a typing job is running"));
    // The token is single use: it is consumed whatever the outcome, so a refused attempt needs a new confirmation.
    const auto it = m_tokens.find(session.deviceId());
    std::optional<Token> token;
    if (it != m_tokens.end()) {
        token = *it;
        m_tokens.erase(it);
    }
    if (!token || token->value != tokenValue || token->age.elapsed() > m_tokenLifetimeMs) {
        return sendError(session, e.id, QStringLiteral("target-changed"), QStringLiteral("target not confirmed or expired"));
    }
    if (!m_gate.open()) {
        return sendError(session, e.id,
                         m_gate.reason() == InputGate::Reason::Locked ? QStringLiteral("locked")
                                                                      : QStringLiteral("unverified"));
    }
    const std::optional<FocusFact> focus = m_focus.current();
    if (!focus) return sendError(session, e.id, QStringLiteral("unverified"), QStringLiteral("focus unknown"));
    if (focus->uuid != token->uuid || focus->revision != token->revision) {
        return sendError(session, e.id, QStringLiteral("target-changed"), QStringLiteral("focus changed"));
    }
    Job job;
    job.session = &session;
    job.device = session.deviceId();
    job.id = e.id;
    job.uuid = token->uuid;
    job.revision = token->revision;
    job.plan = std::move(*plan);
    m_job = std::move(job);
    begin();
}

void VaultTypingFeature::begin()
{
    if (m_portal.active()) return advance();
    if (m_portal.state() == PortalInputSession::State::Idle) {
        // AGENT-GUARD: this raises the one-time desktop approval prompt exactly as input.begin does.
        m_ownsPortal = true;
        Q_EMIT portalBorrowedChanged(true);
    }
    m_job->waitingForPortal = true;
    m_portal.ensure();
}

void VaultTypingFeature::onPortalFailed(PortalInputSession::Failure failure)
{
    if (!m_job || !m_job->waitingForPortal) return;
    const bool denied = failure == PortalInputSession::Failure::Denied;
    abortWith(denied ? QStringLiteral("denied") : QStringLiteral("internal"),
              denied ? QStringLiteral("the desktop user did not approve remote input")
                     : QStringLiteral("remote input is unavailable on the desktop"));
}

void VaultTypingFeature::advance()
{
    while (m_job) {
        if (const QString p = problem(); !p.isEmpty()) return abortWith(p);
        if (m_job->next >= m_job->plan.size()) return finish(QString());
        const qsizetype i = m_job->next++;
        const SecretPlan::Step &step = m_job->plan.at(i);
        switch (step.kind) {
        case SecretPlan::Kind::Text: {
            m_job->textToken = QStringLiteral("vault.type/%1").arg(++m_seq);
            m_guardProblem.clear();
            if (!m_text.enqueueSymbols(m_job->textToken, m_job->plan.takeSymbols(i))) {
                return abortWith(QStringLiteral("internal"), QStringLiteral("could not queue text"));
            }
            return; // continues from TextTyper::finished
        }
        case SecretPlan::Kind::Key:
            if (!m_held.keysym(step.keysym, true) || !m_held.keysym(step.keysym, false)) {
                return abortWith(QStringLiteral("internal"), QStringLiteral("key delivery failed"));
            }
            break;
        case SecretPlan::Kind::Delay:
            m_delay.start(step.delayMs);
            return;
        }
    }
}

void VaultTypingFeature::abortWith(const QString &code, const QString &message)
{
    if (m_job) finish(code, message);
}

void VaultTypingFeature::finish(const QString &code, const QString &message)
{
    if (!m_job) return;
    // AGENT-GUARD: take the job out first. cancel() emits finished(false) synchronously and its handler
    // must see no job, or the same request would be answered twice.
    Job job = std::move(*m_job);
    m_job.reset();
    m_delay.stop();
    m_text.cancel();
    m_held.releaseAll();
    job.plan.wipe();
    if (job.session != nullptr && job.id != 0) {
        if (code.isEmpty()) {
            Envelope ok;
            ok.t = QStringLiteral("ok");
            ok.re = job.id;
            job.session->send(ok);
        } else {
            job.session->send(makeError(code, message, job.id));
        }
    }
    if (m_ownsPortal) {
        m_ownsPortal = false;
        const bool inputUses = m_inputInUse && m_inputInUse();
        Q_EMIT portalBorrowedChanged(false);
        if (!inputUses && m_portal.state() != PortalInputSession::State::Idle) m_portal.close();
    }
}

} // namespace QindaQt::Link
