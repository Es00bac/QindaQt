// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_input/input_sink.h>
#include <qindaqt/services/link_input/restore_token_store.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>

#include <functional>
#include <memory>
#include <vector>

namespace QindaQt::Link {

struct PortalOptions {
    QString service = QStringLiteral("org.freedesktop.portal.Desktop");
    QString path = QStringLiteral("/org/freedesktop/portal/desktop");
    QString tokenFile;          // restore token location; empty disables persistence
    int stepTimeoutMs = 120'000; // the user may take a while to approve
};

// One RemoteDesktop portal session (keyboard + pointer) shared by every
// QindaLink input feature, implementing InputSink.
//
// Ownership: borrows nothing; owns the portal session handle. Threading: GUI
// thread. Error behavior: ensure() is asynchronous; exactly one of
// activated() or failed() follows it, and revoked() follows an active session
// the portal closed.
// AGENT-GUARD: ensure() is the only call that can raise the approval prompt.
// It runs only on a phone's explicit input.begin. No Notify call is sent
// before a zero Start response granted keyboard and pointer.
// AGENT-CONTRACT: the portal side is org.freedesktop.portal.RemoteDesktop and
// .Request (docs/wiki/architecture/qindalink.md); tests register fakes on a
// private bus and pass that bus plus service name here.
class PortalInputSession final : public QObject, public InputSink {
    Q_OBJECT
public:
    enum class State { Idle, Creating, Selecting, Starting, Active };
    enum class Failure { Denied, Unavailable };

    PortalInputSession(QDBusConnection bus, PortalOptions options, QObject *parent = nullptr);
    ~PortalInputSession() override;

    [[nodiscard]] State state() const { return m_state; }
    void ensure();          // starts the handshake from Idle; no effect otherwise
    void close();           // closes the portal session; back to Idle

    // InputSink
    [[nodiscard]] bool active() const override { return m_state == State::Active; }
    bool pointerMotion(double dx, double dy) override;
    bool pointerButton(qint32 evdevCode, bool down) override;
    bool pointerAxis(double dx, double dy, bool finish) override;
    bool pointerAxisDiscrete(quint32 axis, qint32 steps) override;
    bool keysym(qint32 sym, bool down) override;

Q_SIGNALS:
    void activated();
    void failed(QindaQt::Link::PortalInputSession::Failure failure, const QString &reason);
    void revoked();

private Q_SLOTS:
    void onSessionClosed(const QVariantMap &details);

private:
    void request(const QString &method, const QVariantList &args, QVariantMap options,
                 std::function<void(uint, const QVariantMap &)> done);
    void onCreated(uint response, const QVariantMap &results);
    void onSelected(uint response, const QVariantMap &results);
    void onStarted(uint response, const QVariantMap &results);
    bool notify(const QString &method, const QVariantList &args);
    void fail(Failure failure, const QString &reason);
    void resetSession(bool sendClose);

    QDBusConnection m_bus;
    PortalOptions m_options;
    RestoreTokenStore m_tokens;
    State m_state = State::Idle;
    QString m_session;
    QTimer m_timeout;
    quint64 m_generation = 0; // fences replies of an abandoned handshake
    std::vector<std::unique_ptr<QObject>> m_sinks;
};

} // namespace QindaQt::Link
