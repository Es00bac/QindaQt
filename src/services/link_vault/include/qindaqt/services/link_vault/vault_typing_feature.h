// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_focus/focus_source.h>
#include <qindaqt/services/link_input/input_sink.h>
#include <qindaqt/services/link_input/portal_session.h>
#include <qindaqt/services/link_input/text_typer.h>
#include <qindaqt/services/link_transport/feature.h>
#include <qindaqt/services/link_vault/secret_plan.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <functional>
#include <optional>

namespace QindaQt::Link {

// The `vault` capability: `desktop.focus` and `vault.type` (protocol page, "Desktop facts and
// secret typing"; ADR-0440, architecture page "Secret typing").
//
// Flow: the phone asks `desktop.focus`, gets an opaque token bound to (uuid, revision) of the focused
// window for at most 60 s, shows app and title to the user, and after confirmation sends `vault.type`
// with the token. The desktop types only if the focus still matches, re-checks before every step and
// before every 24-symbol batch, and aborts with `target-changed` (after releasing every key) on any
// change.
//
// Ownership: borrows the portal session, the gate and the focus source; they outlive it. Threading:
// GUI thread. Error behavior: every request with an id gets exactly one reply: `ok` or `error` with
// code busy, denied, locked, unverified, target-changed, bad-request or internal.
// AGENT-GUARD: fail closed. No focus fact, a closed gate, an unknown/expired/foreign token or any
// mismatch types nothing. One typing job at a time. The plan (secret keysyms) is wiped on every exit
// path through finish(); never log, copy or describe it.
// AGENT-CONTRACT: shares the RemoteDesktop portal session with InputFeature. If this feature started
// the session it closes it when the job ends and no input controller remains (inputInUse); the
// composition connects portalBorrowedChanged to InputFeature::setPortalBorrowed.
class VaultTypingFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    static constexpr int kTokenLifetimeMs = 60'000;

    VaultTypingFeature(PortalInputSession &portal, InputGate &gate, FocusSource &focus, QObject *parent = nullptr);
    ~VaultTypingFeature() override;

    [[nodiscard]] QString ns() const override { return QStringLiteral("vault"); }
    [[nodiscard]] QStringList extraNamespaces() const override { return {QStringLiteral("desktop")}; }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;

    // Whether the input feature has a controller; if so the portal session is left open after a job.
    void setInputInUse(std::function<bool()> inUse) { m_inputInUse = std::move(inUse); }
    // Test seam: a shorter token lifetime (never longer than kTokenLifetimeMs).
    void setTokenLifetime(int ms) { m_tokenLifetimeMs = qBound(1, ms, kTokenLifetimeMs); }
    [[nodiscard]] bool typing() const { return m_job.has_value(); }
    // Aborts a running job without a reply and closes what this feature opened.
    void shutdown();

Q_SIGNALS:
    void portalBorrowedChanged(bool borrowed);

private:
    struct Token {
        QString value, uuid;
        quint64 revision = 0;
        QElapsedTimer age;
    };
    struct Job {
        Session *session = nullptr; // null once the phone's session closed
        QString device;
        qint64 id = 0;
        QString uuid;
        quint64 revision = 0;
        SecretPlan plan;
        qsizetype next = 0;
        QString textToken;
        bool waitingForPortal = false;
    };

    void focusRequest(Session &session, const Envelope &e);
    void typeRequest(Session &session, const Envelope &e);
    void begin();
    void advance();
    // Empty when typing may continue, else the error code to answer.
    [[nodiscard]] QString problem() const;
    void abortWith(const QString &code, const QString &message = {});
    void finish(const QString &code, const QString &message = {});
    void onPortalFailed(PortalInputSession::Failure failure);
    static void sendError(Session &session, qint64 id, const QString &code, const QString &message = {});

    PortalInputSession &m_portal;
    InputGate &m_gate;
    FocusSource &m_focus;
    HeldInput m_held;
    TextTyper m_text;
    QTimer m_delay;
    QHash<QString, Token> m_tokens; // by device id: at most one outstanding token per phone
    std::optional<Job> m_job;
    std::function<bool()> m_inputInUse;
    QString m_guardProblem;
    quint64 m_seq = 0;
    int m_tokenLifetimeMs = kTokenLifetimeMs;
    bool m_ownsPortal = false;
};

} // namespace QindaQt::Link
