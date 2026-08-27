// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Services::SettingsClient {

class SettingsClient;
struct CommitOutcome;

class DoNotDisturbController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool hasBaseline READ hasBaseline NOTIFY hasBaselineChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool conflict READ conflict NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool canToggle READ canToggle NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

public:
    enum class State { Loading, Ready, Saving, Conflict, Unavailable };
    Q_ENUM(State)

    explicit DoNotDisturbController(SettingsClient &client, QObject *parent = nullptr);

    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }
    [[nodiscard]] bool hasBaseline() const noexcept { return m_hasBaseline; }
    [[nodiscard]] bool loading() const noexcept { return m_state == State::Loading; }
    [[nodiscard]] bool ready() const noexcept { return m_state == State::Ready; }
    [[nodiscard]] bool saving() const noexcept { return m_state == State::Saving; }
    [[nodiscard]] bool conflict() const noexcept { return m_state == State::Conflict; }
    [[nodiscard]] bool unavailable() const noexcept { return m_state == State::Unavailable; }
    [[nodiscard]] bool canToggle() const noexcept { return ready(); }
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] const QString &errorText() const noexcept { return m_error; }

    Q_INVOKABLE bool requestSet(bool enabled);
    Q_INVOKABLE bool applyMyChoice();
    Q_INVOKABLE void retry();

Q_SIGNALS:
    void enabledChanged();
    void hasBaselineChanged();
    void stateChanged();
    void confirmedValue(bool enabled);

private:
    void handleClientState();
    void handleSnapshot();
    void handleCommit(const CommitOutcome &outcome);
    void setState(State state, QString error = {});

    SettingsClient &m_client;
    QString m_error;
    State m_state = State::Loading;
    bool m_enabled = false;
    bool m_hasBaseline = false;
    bool m_requestedValue = false;
    bool m_hasRequestedValue = false;
    bool m_waitingForCommitSnapshot = false;
};

} // namespace QindaQt::Services::SettingsClient
