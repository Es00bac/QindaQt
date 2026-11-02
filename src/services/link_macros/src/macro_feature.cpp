// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_macros/macro_feature.h>
#include <qindaqt/services/link_transport/session.h>

#include <QtCore/QJsonArray>
#include <QtCore/QTimer>

namespace QindaQt::Link::Macros {

MacroFeature::MacroFeature(Hub &hub, MacroCatalog &catalog, InputFeature &input, PortalMacroOutput &output,
                           QObject *parent)
    : QObject(parent), m_hub(hub), m_catalog(catalog), m_input(input), m_output(output), m_runner(output)
{
    connect(&m_hub, &Hub::devicesChanged, this, &MacroFeature::syncReadySessions);
    connect(&m_catalog, &MacroCatalog::changed, this, [this] {
        for (Session *s : std::as_const(m_known)) pushList(*s);
    });
    connect(&m_input, &InputFeature::controllerEnded, this, [this](const QString &device) {
        if (device == m_runDevice) m_runner.cancel();
    });
    connect(&m_runner, &InputActions::MacroRunner::failed, this, [this](const QString &reason) { m_failure = reason; });
    // changed() fires on start and on stop; failed() is emitted just after the
    // stop one, so the outcome is settled on the next event-loop turn.
    connect(&m_runner, &InputActions::MacroRunner::changed, this, [this] {
        if (!m_runner.running()) QTimer::singleShot(0, this, &MacroFeature::settle);
    });
}

void MacroFeature::syncReadySessions()
{
    QSet<QString> ready;
    for (const QJsonValue &d : m_hub.devices()) {
        const QJsonObject o = d.toObject();
        if (!o.value(QStringLiteral("ready")).toBool()) continue;
        const QString id = o.value(QStringLiteral("id")).toString();
        Session *s = m_hub.session(id);
        if (s == nullptr || s->state() != SessionState::Ready || !s->peerHasCap(QStringLiteral("macro"))) continue;
        ready.insert(id);
        if (m_known.value(id) != s) {
            m_known.insert(id, s);
            pushList(*s);
        }
    }
    for (auto it = m_known.begin(); it != m_known.end();) {
        it = ready.contains(it.key()) ? std::next(it) : m_known.erase(it);
    }
}

void MacroFeature::pushList(Session &session)
{
    QJsonArray rows;
    for (const auto &m : m_catalog.macros()) {
        // AGENT-GUARD: id and name only. Steps, text and shortcuts stay on the desktop.
        rows.append(QJsonObject{{QStringLiteral("id"), m.id}, {QStringLiteral("name"), m.name}});
    }
    Envelope e;
    e.t = QStringLiteral("macro.list");
    e.b = QJsonObject{{QStringLiteral("rev"), m_catalog.revision()}, {QStringLiteral("macros"), rows}};
    session.send(e);
}

void MacroFeature::sessionClosed(const QString &deviceId)
{
    m_known.remove(deviceId);
    if (deviceId == m_runDevice) {
        m_runRequest = 0; // nobody left to answer
        m_runner.cancel();
    }
}

void MacroFeature::reply(Session &session, qint64 id, const QString &errorCode, const QString &message)
{
    if (id == 0) return;
    if (errorCode.isEmpty()) {
        Envelope ok;
        ok.t = QStringLiteral("ok");
        ok.re = id;
        session.send(ok);
    } else {
        session.send(makeError(errorCode, message, id));
    }
}

void MacroFeature::handle(Session &session, const Envelope &e)
{
    if (e.t == QLatin1String("macro.run")) return run(session, e);
    if (e.t == QLatin1String("macro.cancel")) {
        if (m_runner.running() && session.deviceId() == m_runDevice) {
            m_cancelRequested = true;
            m_runner.cancel();
        }
        reply(session, e.id, {});
        return;
    }
    if (e.t == QLatin1String("macro.list") && e.id != 0) { // the phone asks for a fresh list
        pushList(session);
        reply(session, e.id, {});
        return;
    }
    if (e.id != 0) reply(session, e.id, QStringLiteral("unknown-type"), QStringLiteral("unknown macro frame"));
}

void MacroFeature::run(Session &session, const Envelope &e)
{
    const QJsonValue idValue = e.b.value(QLatin1String("id"));
    if (!idValue.isString() || idValue.toString().isEmpty() || idValue.toString().size() > 40) {
        reply(session, e.id, QStringLiteral("bad-request"), QStringLiteral("macro id required"));
        return;
    }
    if (m_input.controller() != session.deviceId() || !m_input.flowing()) {
        reply(session, e.id, QStringLiteral("denied"), QStringLiteral("send input.begin and wait for approval first"));
        return;
    }
    if (m_runner.running()) {
        reply(session, e.id, QStringLiteral("busy"), QStringLiteral("a macro is already running"));
        return;
    }
    const auto macro = m_catalog.macro(idValue.toString());
    if (!macro) {
        reply(session, e.id, QStringLiteral("bad-request"), QStringLiteral("unknown macro"));
        return;
    }
    m_failure.clear();
    m_cancelRequested = false;
    m_runDevice = session.deviceId();
    m_runMacro = macro->id;
    m_runRequest = e.id;
    if (!m_runner.start(QStringLiteral("link/") + macro->id, *macro)) {
        m_runRequest = 0;
        m_runDevice.clear();
        reply(session, e.id, QStringLiteral("internal"), QStringLiteral("macro could not start"));
        return;
    }
    pushState(QStringLiteral("running"));
}

void MacroFeature::pushState(const QString &state, const QString &reason)
{
    Session *s = m_hub.session(m_runDevice);
    if (s == nullptr) return;
    QJsonObject b{{QStringLiteral("id"), m_runMacro}, {QStringLiteral("state"), state}};
    if (!reason.isEmpty()) b.insert(QStringLiteral("reason"), reason);
    Envelope e;
    e.t = QStringLiteral("macro.state");
    e.b = b;
    s->send(e);
}

void MacroFeature::settle()
{
    if (m_runner.running() || m_runDevice.isEmpty()) return;
    QString state = QStringLiteral("done"), code, reason;
    if (!m_failure.isEmpty()) {
        state = QStringLiteral("failed");
        code = QStringLiteral("internal");
        reason = m_failure;
    } else if (m_cancelRequested || !m_output.allowed()) {
        state = QStringLiteral("cancelled");
        code = QStringLiteral("internal");
        reason = m_cancelRequested ? QStringLiteral("cancelled") : QStringLiteral("input-lost");
    }
    pushState(state, reason);
    if (m_runRequest != 0) {
        if (Session *s = m_hub.session(m_runDevice)) reply(*s, m_runRequest, code, reason);
    }
    m_runRequest = 0;
    m_runDevice.clear();
    m_runMacro.clear();
}

} // namespace QindaQt::Link::Macros
