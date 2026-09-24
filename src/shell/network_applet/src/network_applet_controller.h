// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/shell/network_applet/network_applet_types.h>
#include <qindaqt/shell/network_applet/network_request_state.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

#include <functional>
#include <optional>

namespace QindaQt::Shell::NetworkApplet
{

// Shell-private adapter from the public Network1 NetworkClient to bounded QML
// values (ADR-0258). It borrows a same-thread client owned by shell
// composition; the client must outlive the controller. Only opaque row ids
// cross into QML, and every mutation re-validates against the current model.
//
// AGENT-CONTRACT: This controller holds at most one request. It reports an
// action as done only after a newer same-owner snapshot shows the requested
// state; refusal, owner loss, and uncertainty retire the request without an
// automatic replay. It has no credential surface: joining a secured network
// relies on the separate network secret agent's own prompt (ADR-0069).
class NetworkAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
    Q_PROPERTY(QString indicator READ indicator NOTIFY stateChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY stateChanged)
    Q_PROPERTY(QString summaryLabel READ summaryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceEpoch READ serviceEpoch NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceRevision READ serviceRevision NOTIFY stateChanged)
    Q_PROPERTY(bool wifiDevicePresent READ wifiDevicePresent NOTIFY stateChanged)
    Q_PROPERTY(bool scanAvailable READ scanAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(QVariantList radioRows READ radioRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList connectionRows READ connectionRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList accessPointRows READ accessPointRows NOTIFY stateChanged)
    Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
    Q_PROPERTY(QString requestPhase READ requestPhase NOTIFY stateChanged)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY stateChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY stateChanged)
    Q_PROPERTY(bool canOpenSettings READ canOpenSettings NOTIFY stateChanged)

public:
    using Launch = std::function<bool()>;

    // Bounded confirmation windows. Connecting a new network may wait on the
    // user typing a password into the secret agent's prompt.
    struct Timing {
        int readbackPollMilliseconds = 1'000;
        int confirmMilliseconds = 15'000;
        int connectConfirmMilliseconds = 90'000;
    };

    NetworkAppletController(Network::Client::NetworkClient *client,
                            bool networkReadGranted, bool networkControlGranted,
                            QObject *parent = nullptr);
    NetworkAppletController(Network::Client::NetworkClient *client,
                            bool networkReadGranted, bool networkControlGranted,
                            Timing timing, QObject *parent = nullptr);
    ~NetworkAppletController() override;

    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString diagnostic() const { return m_model.diagnostic; }
    [[nodiscard]] QString indicator() const;
    [[nodiscard]] QString iconName() const { return m_model.iconName; }
    [[nodiscard]] QString summaryLabel() const { return m_model.summaryLabel; }
    [[nodiscard]] QString accessibleName() const { return m_model.accessibleName; }
    [[nodiscard]] QString accessibleDescription() const
    {
        return m_model.accessibleDescription;
    }
    [[nodiscard]] quint64 serviceEpoch() const noexcept { return m_model.epoch; }
    [[nodiscard]] quint64 serviceRevision() const noexcept { return m_model.revision; }
    [[nodiscard]] bool wifiDevicePresent() const noexcept
    {
        return m_model.wifiDevicePresent;
    }
    [[nodiscard]] bool scanAvailable() const noexcept { return m_model.scanAvailable; }
    [[nodiscard]] bool scanning() const noexcept
    {
        return m_model.scanning
            || (m_request.active() && m_request.target.action == RequestAction::Scan);
    }
    [[nodiscard]] QVariantList radioRows() const;
    [[nodiscard]] QVariantList connectionRows() const;
    [[nodiscard]] QVariantList accessPointRows() const;
    [[nodiscard]] bool operationPending() const noexcept { return m_request.active(); }
    [[nodiscard]] QString requestPhase() const;
    [[nodiscard]] bool feedbackPresent() const noexcept
    {
        return !m_request.feedback.isEmpty();
    }
    [[nodiscard]] QString feedback() const { return m_request.feedback; }
    [[nodiscard]] bool canOpenSettings() const noexcept
    {
        return static_cast<bool>(m_settingsLaunch);
    }
    [[nodiscard]] const NetworkAppletModel &model() const noexcept { return m_model; }
    [[nodiscard]] const RequestState &request() const noexcept { return m_request; }

    // Shell composition injects the Settings route launcher; the controller
    // never names a process itself.
    void setSettingsLaunch(Launch launch);

    Q_INVOKABLE void setExpanded(bool expanded);
    Q_INVOKABLE bool requestRadio(const QString &radioId, bool enable);
    Q_INVOKABLE bool requestConnect(const QString &accessPointId);
    Q_INVOKABLE bool requestDisconnect(const QString &connectionId);
    Q_INVOKABLE bool requestScan();
    Q_INVOKABLE bool openSettings();
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateChanged();

private:
    void reproject();
    void handleOperationFinished(const Network::OperationResult &result);
    void handleOperationUncertain(const QString &message);
    void settle(const RequestState &next);
    void pollReadback();
    [[nodiscard]] bool dispatch(const RequestTarget &target,
                                const std::function<bool(QString *)> &send);
    [[nodiscard]] bool refuse(const RequestTarget &target, const QString &reasonCode);
    [[nodiscard]] bool rowPending(RequestAction action, const QString &id) const;

    Network::Client::NetworkClient *m_client = nullptr;
    bool m_readGranted = false;
    bool m_controlGranted = false;
    bool m_expanded = false;
    Timing m_timing;
    NetworkAppletModel m_model;
    RequestState m_request;
    // The request as last published; with m_model it suppresses no-op
    // notifications.
    RequestState m_publishedRequest;
    QString m_lastOwner;
    Launch m_settingsLaunch;
    QTimer m_readbackPoll;
    QTimer m_confirmDeadline;
};

} // namespace QindaQt::Shell::NetworkApplet
