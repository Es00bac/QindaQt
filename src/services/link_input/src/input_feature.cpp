// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_transport/session.h>

#include <algorithm>

namespace QindaQt::Link {
namespace {
constexpr double kRatePerSecond = 240; // desktop cap; the phone coalesces to 120/s
constexpr double kBurst = 60;
}

InputFeature::InputFeature(PortalInputSession &portal, InputGate &gate, QObject *parent)
    : QObject(parent), m_portal(portal), m_gate(gate), m_held(portal), m_text(portal)
{
    m_clock.start();
    connect(&m_portal, &PortalInputSession::activated, this, &InputFeature::onActivated);
    connect(&m_portal, &PortalInputSession::failed, this,
            [this](PortalInputSession::Failure f, const QString &) { onFailed(f); });
    connect(&m_portal, &PortalInputSession::revoked, this, [this] { endController(QStringLiteral("revoked"), true); });
    connect(&m_gate, &InputGate::changed, this, &InputFeature::onGateChanged);
    connect(&m_text, &TextTyper::finished, this, [this](const QString &token, bool ok) {
        if (token != m_textToken) return;
        reply(m_textId, ok ? QString() : QStringLiteral("internal"), QStringLiteral("text delivery failed"));
        m_textId = 0;
        m_textToken.clear();
    });
}

InputFeature::~InputFeature() { m_held.releaseAll(); }

bool InputFeature::flowing() const
{
    return m_phase == Phase::Active && m_portal.active() && m_gate.open();
}

void InputFeature::handle(Session &session, const Envelope &e)
{
    if (e.t == QLatin1String("input.begin")) return begin(session, e.id);
    if (e.t == QLatin1String("input.end")) return end(session, e.id);
    if (e.t == QLatin1String("input.state")) return; // desktop to phone only
    // An unknown input.* with an id is answered by the caller's default; here
    // only the five event frames are valid.
    deliver(session, e);
}

void InputFeature::reply(qint64 id, const QString &errorCode, const QString &message)
{
    if (id == 0 || m_session == nullptr) return;
    if (errorCode.isEmpty()) {
        Envelope ok;
        ok.t = QStringLiteral("ok");
        ok.re = id;
        m_session->send(ok);
    } else {
        m_session->send(makeError(errorCode, message, id));
    }
}

void InputFeature::pushState(const QString &state)
{
    if (m_session == nullptr) return;
    Envelope e;
    e.t = QStringLiteral("input.state");
    e.b = QJsonObject{{QStringLiteral("state"), state}};
    m_session->send(e);
}

void InputFeature::begin(Session &session, qint64 id)
{
    if (!m_device.isEmpty() && m_device != session.deviceId()) {
        session.send(makeError(QStringLiteral("busy"), QStringLiteral("another phone controls the desktop"), id));
        return;
    }
    m_device = session.deviceId();
    m_session = &session;
    if (!m_gate.open()) {
        reply(id, QStringLiteral("busy"), QStringLiteral("locked"));
        pushState(QStringLiteral("locked"));
        return;
    }
    if (m_portal.active()) {
        m_phase = Phase::Active;
        reply(id, {});
        pushState(QStringLiteral("active"));
        return;
    }
    if (m_phase == Phase::Pending) {
        // A repeated begin during the handshake: the earlier request is answered busy.
        reply(m_beginId, QStringLiteral("busy"), QStringLiteral("superseded"));
    }
    m_phase = Phase::Pending;
    m_beginId = id;
    pushState(QStringLiteral("pending"));
    m_portal.ensure();
}

void InputFeature::onActivated()
{
    if (m_phase != Phase::Pending) {
        m_portal.close(); // nobody is waiting for it
        return;
    }
    m_phase = Phase::Active;
    const qint64 id = std::exchange(m_beginId, 0);
    reply(id, {});
    pushState(m_gate.open() ? QStringLiteral("active") : QStringLiteral("locked"));
}

void InputFeature::onFailed(PortalInputSession::Failure failure)
{
    if (m_phase != Phase::Pending) return;
    const qint64 id = std::exchange(m_beginId, 0);
    const bool denied = failure == PortalInputSession::Failure::Denied;
    reply(id, denied ? QStringLiteral("denied") : QStringLiteral("internal"),
          denied ? QStringLiteral("the desktop user did not approve remote input")
                 : QStringLiteral("remote input is unavailable on the desktop"));
    pushState(denied ? QStringLiteral("denied") : QStringLiteral("unavailable"));
    m_phase = Phase::None;
    m_device.clear();
    m_session = nullptr;
}

void InputFeature::end(Session &session, qint64 id)
{
    if (m_device == session.deviceId()) endController(QStringLiteral("ended"), false);
    if (id != 0) {
        Envelope ok;
        ok.t = QStringLiteral("ok");
        ok.re = id;
        session.send(ok);
    }
}

void InputFeature::endController(const QString &reason, bool notifyPhone)
{
    if (m_device.isEmpty()) return;
    const QString who = std::exchange(m_device, QString());
    m_text.cancel();
    m_held.releaseAll();
    if (notifyPhone && m_session != nullptr) {
        Envelope e;
        e.t = QStringLiteral("input.end");
        e.b = QJsonObject{{QStringLiteral("reason"), reason}};
        m_session->send(e);
    }
    m_phase = Phase::None;
    m_beginId = m_textId = 0;
    m_textToken.clear();
    m_session = nullptr;
    // The portal session closes with its controller so the compositor's
    // "remote control" indicator never outlives the phone's use.
    if (m_portal.state() != PortalInputSession::State::Idle) m_portal.close();
    Q_EMIT controllerEnded(who);
}

void InputFeature::sessionClosed(const QString &deviceId)
{
    if (deviceId == m_device) endController(QStringLiteral("disconnected"), false);
}

void InputFeature::onGateChanged()
{
    if (m_device.isEmpty()) return;
    if (!m_gate.open()) {
        m_text.cancel();
        m_held.releaseAll();
        pushState(QStringLiteral("locked"));
    } else if (m_phase == Phase::Active && m_portal.active()) {
        pushState(QStringLiteral("active"));
    }
}

bool InputFeature::allowRate()
{
    const double elapsed = double(m_clock.restart()) / 1000.0;
    m_tokens = std::min(kBurst, m_tokens + elapsed * kRatePerSecond);
    if (m_tokens < 1) return false;
    m_tokens -= 1;
    return true;
}

void InputFeature::deliver(Session &session, const Envelope &e)
{
    namespace F = InputFrames;
    if (session.deviceId() != m_device || !flowing()) return; // dropped: not the controller or not READY for input
    const auto event = F::parse(e.t, e.b);
    if (!event) {
        if (e.id != 0) session.send(makeError(QStringLiteral("bad-request"), QStringLiteral("invalid input frame"), e.id));
        return;
    }
    if (const auto *p = std::get_if<F::Pointer>(&*event)) {
        if (allowRate()) m_held.pointerMotion(p->dx, p->dy);
    } else if (const auto *b = std::get_if<F::Button>(&*event)) {
        if (b->state != F::ButtonState::Up) m_held.pointerButton(b->evdev, true);
        if (b->state != F::ButtonState::Down) m_held.pointerButton(b->evdev, false);
    } else if (const auto *s = std::get_if<F::Scroll>(&*event)) {
        if (!allowRate()) return;
        if (s->discrete) {
            if (s->dy != 0) m_held.pointerAxisDiscrete(0, qint32(s->dy));
            if (s->dx != 0) m_held.pointerAxisDiscrete(1, qint32(s->dx));
        } else {
            m_held.pointerAxis(s->dx, s->dy, s->finish);
        }
    } else if (const auto *k = std::get_if<F::Key>(&*event)) {
        if (k->state != F::KeyState::Up) m_held.keysym(k->keysym, true);
        if (k->state != F::KeyState::Down) m_held.keysym(k->keysym, false);
    } else if (const auto *t = std::get_if<F::Text>(&*event)) {
        const QString token = QStringLiteral("input.text/%1").arg(++m_textSeq);
        if (!m_textToken.isEmpty() || !m_text.enqueue(token, t->text)) {
            if (e.id != 0) session.send(makeError(QStringLiteral("busy"), QStringLiteral("text queue full"), e.id));
            return;
        }
        m_textId = e.id;
        m_textToken = token;
    }
}

} // namespace QindaQt::Link
