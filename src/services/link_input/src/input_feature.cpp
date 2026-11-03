// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_transport/session.h>

#include <algorithm>

namespace QindaQt::Link {
namespace {
constexpr double kRatePerSecond = 240; // desktop cap; the phone coalesces to 120/s
constexpr double kBurst = 60;
constexpr double kKeyRatePerSecond = 100; // key and button presses; releases are never limited
constexpr double kKeyBurst = 30;
}

InputFeature::InputFeature(PortalInputSession &portal, InputGate &gate, QObject *parent)
    : QObject(parent), m_portal(portal), m_gate(gate), m_held(portal), m_text(portal)
{
    m_clock.start();
    // AGENT-GUARD: backstop for a lost key-up or button-up (a dropped frame, a failed portal call).
    m_watchdog.setInterval(1000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] { m_held.releaseOlderThan(m_holdLimitMs); });
    m_watchdog.start();
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

void InputFeature::shutdown()
{
    endController(QStringLiteral("shutdown"), true);
    m_held.releaseAll();
    if (m_portal.state() != PortalInputSession::State::Idle) m_portal.close();
}

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
        Envelope state; // the reply alone never reaches the phone's UI
        state.t = QStringLiteral("input.state");
        state.b = QJsonObject{{QStringLiteral("state"), QStringLiteral("busy")}};
        session.send(state);
        return;
    }
    if (!m_gate.open()) {
        // Not claimed: a locked desktop must not keep a phone as controller.
        session.send(makeError(QStringLiteral("busy"), QStringLiteral("locked"), id));
        Envelope state;
        state.t = QStringLiteral("input.state");
        state.b = QJsonObject{{QStringLiteral("state"), QStringLiteral("locked")}};
        session.send(state);
        return;
    }
    m_device = session.deviceId();
    m_session = &session;
    m_held.releaseAll(); // a fresh begin never inherits anything held by an earlier one
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
    // AGENT-GUARD: ordering. Dependents (the macro runner) release what they
    // hold while the portal session is still active; only then does it close.
    Q_EMIT controllerEnded(who);
    m_held.releaseAll();
    // The portal session closes with its controller so the compositor's
    // "remote control" indicator never outlives the phone's use.
    if (m_portal.state() != PortalInputSession::State::Idle) m_portal.close();
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

bool InputFeature::take(double &tokens, qint64 &last, double rate, double burst)
{
    const qint64 now = m_clock.elapsed();
    tokens = std::min(burst, tokens + double(now - last) / 1000.0 * rate);
    last = now;
    if (tokens < 1) return false;
    tokens -= 1;
    return true;
}

bool InputFeature::allowRate() { return take(m_tokens, m_lastMotion, kRatePerSecond, kBurst); }
bool InputFeature::allowKeyRate() { return take(m_keyTokens, m_lastKey, kKeyRatePerSecond, kKeyBurst); }

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
        // Releases are never rate limited: dropping one would leave the button down.
        if (b->state != F::ButtonState::Up && !allowKeyRate()) return;
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
        if (k->state != F::KeyState::Up && !allowKeyRate()) return; // releases are exempt, see above
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
