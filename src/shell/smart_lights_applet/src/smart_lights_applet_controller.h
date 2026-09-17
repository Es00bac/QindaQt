// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/smart_lights_store/configuration_store.h>
#include <qindaqt/services/wiz_client/wiz_client.h>
#include <qindaqt/shell/smart_lights_applet/smart_lights_applet_presentation.h>
#include <qindaqt/shell/smart_lights_applet/smart_lights_request_state.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace QindaQt::Shell::SmartLightsApplet
{

// Shell-private adapter from the Wiz client and the stored configuration to
// bounded QML values.
//
// AGENT-CONTRACT: the client and the store are borrowed from shell
// composition, share this object's thread, and must outlive it. Only opaque
// row tokens cross into QML: the mapping from token to device MAC never leaves
// this class, so no QML file, and nothing a QML file could log, learns a
// hardware address.
//
// AGENT-GUARD: every control path runs through beginSmartLightRequest() before
// it reaches the client, even though the client validates again. The duplicate
// admission is what lets the projection promise that an enabled control is a
// dispatchable one.
class SmartLightsAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
    Q_PROPERTY(QString summaryLabel READ summaryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceEpoch READ serviceEpoch NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceRevision READ serviceRevision NOTIFY stateChanged)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList presetRows READ presetRows NOTIFY stateChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY stateChanged)
    Q_PROPERTY(int onCount READ onCount NOTIFY stateChanged)
    Q_PROPERTY(bool controlAvailable READ controlAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY stateChanged)
    Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
    SmartLightsAppletController(Wiz::WizClient *client,
                                SmartLights::ConfigurationStore *store,
                                bool readGranted, bool controlGranted,
                                QObject *parent = nullptr);
    ~SmartLightsAppletController() override;

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
    [[nodiscard]] QVariantList deviceRows() const;
    [[nodiscard]] QVariantList presetRows() const;
    [[nodiscard]] int deviceCount() const noexcept
    {
        return static_cast<int>(m_model.devices.size());
    }
    [[nodiscard]] int onCount() const noexcept { return m_model.onCount; }
    [[nodiscard]] bool controlAvailable() const noexcept
    {
        return m_model.controlGranted && m_model.phase == ServicePhase::Ready;
    }
    [[nodiscard]] bool discovering() const noexcept { return m_model.discovering; }
    [[nodiscard]] bool operationPending() const noexcept { return !m_pending.isEmpty(); }
    [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
    [[nodiscard]] QString feedback() const { return m_feedback; }

    // Opening the popup asks for fresh truth; closing it does not stop the
    // client, because the panel chip keeps showing how many lights are on.
    Q_INVOKABLE void setExpanded(bool expanded);

    Q_INVOKABLE bool requestPower(const QString &rowId, bool on);
    Q_INVOKABLE bool requestAllPower(bool on);
    Q_INVOKABLE bool requestBrightness(const QString &rowId, int percent);
    Q_INVOKABLE bool requestTemperature(const QString &rowId, int kelvin);
    Q_INVOKABLE bool requestColor(const QString &rowId, int red, int green, int blue);
    Q_INVOKABLE bool requestScene(const QString &rowId, int sceneId);
    Q_INVOKABLE bool requestSpeed(const QString &rowId, int percent);
    Q_INVOKABLE bool requestRefresh();
    Q_INVOKABLE bool requestDiscovery();

    // Scene choices this exact luminaire can run, as {sceneId, name, dynamic}.
    Q_INVOKABLE QVariantList sceneOptions(const QString &rowId) const;

    Q_INVOKABLE bool requestRename(const QString &rowId, const QString &label);
    Q_INVOKABLE bool requestForget(const QString &rowId);

    Q_INVOKABLE bool savePreset(const QString &name);
    Q_INVOKABLE bool applyPreset(const QString &presetId);
    Q_INVOKABLE bool deletePreset(const QString &presetId);

    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateChanged();
    void feedbackChanged();

private:
    void reproject();
    void adoptStoredConfiguration();
    void persistConfiguration();
    void handleSnapshotChanged();
    void handleOperationCompleted(quint64 requestId, const Wiz::OperationResult &result);
    void publishFeedback(const QString &message);
    [[nodiscard]] bool dispatch(const Wiz::OperationRequest &request);
    [[nodiscard]] QString macForRow(const QString &rowId) const;
    [[nodiscard]] QString rowIdForMac(const QString &mac);
    [[nodiscard]] std::optional<Wiz::Device> deviceForRow(const QString &rowId) const;
    // Captures what a device is doing now as a replayable intent, or nothing
    // when its state is not known well enough to be worth storing.
    [[nodiscard]] std::optional<Wiz::StateRequest> captureState(
        const Wiz::Device &device) const;

    Wiz::WizClient *m_client = nullptr;
    SmartLights::ConfigurationStore *m_store = nullptr;
    bool m_readGranted = false;
    bool m_controlGranted = false;
    bool m_expanded = false;
    SmartLightsAppletModel m_model;
    SmartLights::StoredConfiguration m_configuration;
    // Row tokens are assigned once per device per session and never reused, so
    // a row that vanishes cannot silently become a different light.
    QHash<QString, QString> m_rowIds;
    QHash<QString, QString> m_rowMacs;
    quint64 m_nextRowSerial = 1;
    // Requests dispatched and not yet completed, keyed by client request id.
    QHash<quint64, RequestState> m_pending;
    QString m_feedback;
};

} // namespace QindaQt::Shell::SmartLightsApplet
