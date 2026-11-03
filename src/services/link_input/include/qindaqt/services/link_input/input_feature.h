// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_input/input_frames.h>
#include <qindaqt/services/link_input/portal_session.h>
#include <qindaqt/services/link_input/text_typer.h>
#include <qindaqt/services/link_transport/feature.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace QindaQt::Link {

// The `input` capability (protocol page, "Remote input (input.*)").
//
// Exactly one phone at a time controls the desktop ("controller"). The
// controller must send input.begin, which is the only thing that can start the
// RemoteDesktop portal session and so the only thing that can raise its
// approval prompt. Everything else is dropped until the portal is Active, the
// gate is open and the frame came from the controller.
//
// Ownership: borrows the portal session, the gate and (through them) nothing
// else; they must outlive this object. Threading: GUI thread.
// AGENT-GUARD: a disconnect, input.end, lock, revoke or portal loss must call
// releaseAll(); a missed path leaves a key or button stuck down on the desktop.
// AGENT-CONTRACT: MacroFeature asks controller()/active() before it runs a
// macro and listens to controllerEnded() to cancel; keep both stable.
class InputFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    InputFeature(PortalInputSession &portal, InputGate &gate, QObject *parent = nullptr);
    ~InputFeature() override;

    [[nodiscard]] QString ns() const override { return QStringLiteral("input"); }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;

    // Device id of the controlling phone ("" when none) and whether input may
    // flow right now (portal active, gate open).
    [[nodiscard]] QString controller() const { return m_device; }
    [[nodiscard]] bool flowing() const;
    // A key or button held longer than this without being pressed again is released (default 30 s).
    void setHoldLimit(int ms) { m_holdLimitMs = ms; }
    [[nodiscard]] bool holding() const { return m_held.holding(); }
    // Orderly shutdown: release everything through the still-active portal, then close it.
    void shutdown();

Q_SIGNALS:
    void controllerEnded(const QString &deviceId);

private:
    enum class Phase { None, Pending, Active };

    void begin(Session &session, qint64 id);
    void end(Session &session, qint64 id);
    void deliver(Session &session, const Envelope &e);
    void onActivated();
    void onFailed(PortalInputSession::Failure failure);
    void onGateChanged();
    void endController(const QString &reason, bool notifyPhone);
    void pushState(const QString &state);
    void reply(qint64 id, const QString &errorCode, const QString &message = {});
    [[nodiscard]] bool allowRate();
    [[nodiscard]] bool allowKeyRate();
    [[nodiscard]] bool take(double &tokens, qint64 &last, double rate, double burst);

    PortalInputSession &m_portal;
    InputGate &m_gate;
    HeldInput m_held;
    TextTyper m_text;
    Phase m_phase = Phase::None;
    QString m_device;
    Session *m_session = nullptr;
    qint64 m_beginId = 0;
    qint64 m_textId = 0;
    QString m_textToken; // the single in-flight input.text job
    quint64 m_textSeq = 0;
    double m_tokens = 60;
    double m_keyTokens = 30;
    qint64 m_lastMotion = 0, m_lastKey = 0;
    int m_holdLimitMs = 30'000;
    QTimer m_watchdog;
    QElapsedTimer m_clock;
};

} // namespace QindaQt::Link
