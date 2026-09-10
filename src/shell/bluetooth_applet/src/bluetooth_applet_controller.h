// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>
#include <qindaqt/shell/bluetooth_applet/bluetooth_applet_types.h>
#include <qindaqt/shell/bluetooth_applet/bluetooth_request_state.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <optional>

namespace QindaQt::Shell::BluetoothApplet
{

// Shell-private adapter from the exact-owner BluetoothClient to bounded QML
// values. It borrows a same-thread client owned by shell composition; that
// client must outlive the controller. Only opaque row IDs cross into QML.
//
// AGENT-CONTRACT: This controller owns at most one caller-scoped discovery
// lease. Popup close defers a release behind any current operation, owner loss
// retires the old lease, and neither failure nor uncertainty is auto-replayed.
class BluetoothAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
    Q_PROPERTY(QString summaryLabel READ summaryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceEpoch READ serviceEpoch NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceRevision READ serviceRevision NOTIFY stateChanged)
    Q_PROPERTY(QVariantList adapterRows READ adapterRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY stateChanged)
    Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
    Q_PROPERTY(bool discoveryLeaseHeld READ discoveryLeaseHeld NOTIFY stateChanged)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(bool pairingPromptVisible READ pairingPromptVisible NOTIFY stateChanged)
    Q_PROPERTY(QString pairingPromptText READ pairingPromptText NOTIFY stateChanged)
    Q_PROPERTY(bool pairingConfirmationAvailable READ pairingConfirmationAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool pairingPasskeyEntryAvailable READ pairingPasskeyEntryAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool pairingPinEntryAvailable READ pairingPinEntryAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool pairingReplyPending READ pairingReplyPending NOTIFY stateChanged)

public:
    explicit BluetoothAppletController(Bluetooth::BluetoothClient *client,
                                       bool bluetoothReadGranted,
                                       bool bluetoothControlGranted,
                                       QObject *parent = nullptr);
    ~BluetoothAppletController() override;

    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString diagnostic() const { return m_model.diagnostic; }
    [[nodiscard]] QString summaryLabel() const { return m_model.summaryLabel; }
    [[nodiscard]] QString accessibleName() const { return m_model.accessibleName; }
    [[nodiscard]] QString accessibleDescription() const
    {
        return m_model.accessibleDescription;
    }
    [[nodiscard]] quint64 serviceEpoch() const noexcept { return m_model.epoch; }
    [[nodiscard]] quint64 serviceRevision() const noexcept { return m_model.revision; }
    [[nodiscard]] QVariantList adapterRows() const;
    [[nodiscard]] QVariantList deviceRows() const;
    [[nodiscard]] bool operationPending() const noexcept
    {
        return requestInFlight() || pairingReplyPending()
            || m_successConvergence.has_value();
    }
    [[nodiscard]] bool discoveryLeaseHeld() const noexcept
    {
        return m_discoveryLease.has_value();
    }
    [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
    [[nodiscard]] QString feedback() const { return m_feedback; }
    [[nodiscard]] bool pairingPromptVisible() const noexcept;
    [[nodiscard]] QString pairingPromptText() const;
    [[nodiscard]] bool pairingConfirmationAvailable() const noexcept;
    [[nodiscard]] bool pairingPasskeyEntryAvailable() const noexcept;
    [[nodiscard]] bool pairingPinEntryAvailable() const noexcept;
    [[nodiscard]] bool pairingReplyPending() const noexcept {
        return m_promptRequestId != 0;
    }

    Q_INVOKABLE void setExpanded(bool expanded);
    Q_INVOKABLE bool requestAdapterPower(const QString &adapterId, bool powered);
    Q_INVOKABLE bool requestDiscovery(const QString &adapterId, bool enabled);
    Q_INVOKABLE bool requestDeviceConnection(const QString &deviceId, bool connected);
    Q_INVOKABLE bool requestPairing(const QString &deviceId);
    Q_INVOKABLE bool requestPairingCancel();
    Q_INVOKABLE bool requestRemoval(const QString &deviceId);
    Q_INVOKABLE bool requestTrusted(const QString &deviceId, bool trusted);
    Q_INVOKABLE bool confirmPrompt();
    Q_INVOKABLE bool cancelPrompt();
    Q_INVOKABLE bool submitPasskey(const QString &text);
    Q_INVOKABLE bool submitPin(const QString &text);
    Q_INVOKABLE void clearFeedback();

    // Shell composition calls this before stopping the client. The release is
    // best effort at process teardown; normal popup close retains the object to
    // observe its typed completion.
    void prepareForShutdown();

Q_SIGNALS:
    void stateChanged();
    void feedbackChanged();

private:
    struct SuccessConvergence {
        QString owner;
        quint64 epoch = 0;
        quint64 minimumRevision = 0;
    };

    [[nodiscard]] bool requestInFlight() const noexcept
    {
        return m_requestId != 0 && m_request.pending();
    }
    void reproject();
    void handleOperationCompleted(quint64 requestId,
                                  const Bluetooth::OperationResult &result);
    void handlePromptCompleted(const Bluetooth::OperationResult &result);
    void publishFeedback(const QString &message);
    [[nodiscard]] bool presentationOwnerAvailable() const noexcept;
    [[nodiscard]] std::optional<Bluetooth::Adapter> findAdapter(
        const QString &rowId) const;
    [[nodiscard]] std::optional<Bluetooth::Device> findDevice(
        const QString &rowId) const;
    [[nodiscard]] bool dispatch(const RequestState &request);
    void releaseDiscoveryAfterSerialization();
    void retireLeaseIfAuthorityEnded();
    void observeSuccessConvergence();

    Bluetooth::BluetoothClient *m_client = nullptr;
    bool m_bluetoothReadGranted = false;
    bool m_bluetoothControlGranted = false;
    bool m_expanded = false;
    bool m_releaseAfterPending = false;
    // AGENT-GUARD: A completed or locally rejected release consumes the
    // current automatic close/shutdown intent. Only a later explicit action
    // may clear this block; otherwise a closed popup can replay forever.
    bool m_automaticReleaseBlocked = false;
    bool m_shuttingDown = false;
    BluetoothAppletModel m_model;
    RequestState m_request;
    quint64 m_requestId = 0;
    quint64 m_promptRequestId = 0;
    QString m_promptOwner;
    quint64 m_promptEpoch = 0;
    QString m_pendingOwner;
    std::optional<SuccessConvergence> m_successConvergence;
    std::optional<Bluetooth::Handle> m_discoveryLease;
    QString m_discoveryLeaseOwner;
    quint64 m_discoveryLeaseMinimumRevision = 0;
    QString m_feedback;
};

} // namespace QindaQt::Shell::BluetoothApplet
