// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"

#include <qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h>

#include <QtCore/QVariantMap>

#include <algorithm>

namespace QindaQt::Shell::BluetoothApplet
{
namespace
{

QString phaseToken(const ServicePhase phase)
{
    switch (phase) {
    case ServicePhase::Loading:
        return QStringLiteral("loading");
    case ServicePhase::Ready:
        return QStringLiteral("ready");
    case ServicePhase::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

} // namespace

BluetoothAppletController::BluetoothAppletController(
    Bluetooth::BluetoothClient *client,
    const bool bluetoothReadGranted,
    const bool bluetoothControlGranted,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_bluetoothReadGranted(bluetoothReadGranted)
    , m_bluetoothControlGranted(bluetoothReadGranted && bluetoothControlGranted)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_client->thread() == thread());
    connect(m_client, &Bluetooth::BluetoothClient::stateChanged,
            this, &BluetoothAppletController::reproject);
    connect(m_client, &Bluetooth::BluetoothClient::snapshotChanged,
            this, &BluetoothAppletController::reproject);
    connect(m_client, &Bluetooth::BluetoothClient::operationCompleted,
            this, &BluetoothAppletController::handleOperationCompleted);
    reproject();
}

BluetoothAppletController::~BluetoothAppletController()
{
    prepareForShutdown();
}

QString BluetoothAppletController::phase() const
{
    return phaseToken(m_model.phase);
}

QVariantList BluetoothAppletController::adapterRows() const
{
    QVariantList rows;
    rows.reserve(m_model.adapters.size());
    for (const AdapterRow &row : m_model.adapters) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("powered"), row.powered},
            {QStringLiteral("discovering"), row.discovering},
            {QStringLiteral("canSetPowered"),
             row.canSetPowered && !operationPending()},
            {QStringLiteral("canAcquireDiscovery"),
             row.canAcquireDiscovery && !operationPending()},
            {QStringLiteral("canReleaseDiscovery"),
             row.canReleaseDiscovery && !operationPending()},
            {QStringLiteral("pending"), operationPending()
                 && adapterRowId(m_request.operation.target) == row.id},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"), row.accessibleDescription},
        });
    }
    return rows;
}

QVariantList BluetoothAppletController::deviceRows() const
{
    QVariantList rows;
    rows.reserve(m_model.devices.size());
    for (const DeviceRow &row : m_model.devices) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("adapterId"), row.adapterId},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("classLabel"), row.classLabel},
            {QStringLiteral("paired"), row.paired},
            {QStringLiteral("connected"), row.connected},
            {QStringLiteral("batteryKnown"), row.batteryKnown},
            {QStringLiteral("batteryPercent"), row.batteryPercent},
            {QStringLiteral("signalKnown"), row.signalKnown},
            {QStringLiteral("signalDbm"), row.signalDbm},
            {QStringLiteral("canConnect"), row.canConnect && !operationPending()},
            {QStringLiteral("canDisconnect"),
             row.canDisconnect && !operationPending()},
            {QStringLiteral("pending"), operationPending()
                 && deviceRowId(m_request.operation.target) == row.id},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"), row.accessibleDescription},
        });
    }
    return rows;
}

bool BluetoothAppletController::presentationOwnerAvailable() const noexcept
{
    return m_bluetoothReadGranted
        && !m_client->owner().isEmpty()
        && m_client->hasSnapshot()
        && m_client->state() == Bluetooth::ClientState::Ready;
}

std::optional<Bluetooth::Adapter> BluetoothAppletController::findAdapter(
    const QString &rowId) const
{
    if (!presentationOwnerAvailable()) {
        return std::nullopt;
    }
    const Bluetooth::Snapshot snapshot = m_client->snapshot();
    const auto it = std::ranges::find_if(
        snapshot.adapters, [&rowId](const Bluetooth::Adapter &adapter) {
            return adapterRowId(adapter.handle) == rowId;
        });
    return it == snapshot.adapters.cend()
        ? std::nullopt : std::optional<Bluetooth::Adapter>(*it);
}

std::optional<Bluetooth::Device> BluetoothAppletController::findDevice(
    const QString &rowId) const
{
    if (!presentationOwnerAvailable()) {
        return std::nullopt;
    }
    const Bluetooth::Snapshot snapshot = m_client->snapshot();
    const auto it = std::ranges::find_if(
        snapshot.devices, [&rowId](const Bluetooth::Device &device) {
            return deviceRowId(device.handle) == rowId;
        });
    return it == snapshot.devices.cend()
        ? std::nullopt : std::optional<Bluetooth::Device>(*it);
}

void BluetoothAppletController::publishFeedback(const QString &message)
{
    if (message == m_feedback) {
        return;
    }
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

void BluetoothAppletController::clearFeedback()
{
    publishFeedback({});
}

bool BluetoothAppletController::dispatch(const RequestState &request)
{
    if (!request.pending() || operationPending()) {
        publishFeedback(request.feedback.isEmpty()
                            ? tr("A Bluetooth operation is already in progress.")
                            : request.feedback);
        return false;
    }
    quint64 requestId = 0;
    switch (request.operation.kind) {
    case Bluetooth::OperationKind::SetAdapterPower:
        requestId = m_client->setAdapterPower(request.operation.target,
                                              request.operation.powered);
        break;
    case Bluetooth::OperationKind::AcquireDiscovery:
        requestId = m_client->acquireDiscovery(request.operation.target);
        break;
    case Bluetooth::OperationKind::ReleaseDiscovery:
        requestId = m_client->releaseDiscovery(request.operation.target);
        break;
    case Bluetooth::OperationKind::Connect:
        requestId = m_client->connectDevice(request.operation.target);
        break;
    case Bluetooth::OperationKind::Disconnect:
        requestId = m_client->disconnectDevice(request.operation.target);
        break;
    }
    if (requestId == 0) {
        publishFeedback(tr("The Bluetooth request could not be sent."));
        return false;
    }
    m_request = request;
    m_requestId = requestId;
    m_pendingOwner = m_client->owner();
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

bool BluetoothAppletController::requestAdapterPower(const QString &adapterId,
                                                     const bool powered)
{
    const std::optional<Bluetooth::Adapter> adapter = findAdapter(adapterId);
    if (!adapter.has_value()) {
        publishFeedback(tr("That Bluetooth adapter is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::SetAdapterPower,
        .target = adapter->handle,
        .powered = powered,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    return dispatch(request);
}

bool BluetoothAppletController::requestDiscovery(const QString &adapterId,
                                                  const bool enabled)
{
    const std::optional<Bluetooth::Adapter> adapter = findAdapter(adapterId);
    if (!adapter.has_value()) {
        publishFeedback(tr("That Bluetooth adapter is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = enabled ? Bluetooth::OperationKind::AcquireDiscovery
                        : Bluetooth::OperationKind::ReleaseDiscovery,
        .target = adapter->handle,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    const bool dispatched = dispatch(request);
    if (dispatched && !enabled) {
        m_automaticReleaseBlocked = false;
    }
    return dispatched;
}

bool BluetoothAppletController::requestDeviceConnection(const QString &deviceId,
                                                         const bool connected)
{
    const std::optional<Bluetooth::Device> device = findDevice(deviceId);
    if (!device.has_value()) {
        publishFeedback(tr("That Bluetooth device is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = connected ? Bluetooth::OperationKind::Connect
                          : Bluetooth::OperationKind::Disconnect,
        .target = device->handle,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    return dispatch(request);
}

void BluetoothAppletController::setExpanded(const bool expanded)
{
    const bool newlyClosed = m_expanded && !expanded;
    m_expanded = expanded;
    if (expanded) {
        return;
    }
    if (newlyClosed || m_shuttingDown) {
        m_automaticReleaseBlocked = false;
    }
    if (operationPending()) {
        if (m_discoveryLease.has_value()
            || m_request.operation.kind == Bluetooth::OperationKind::AcquireDiscovery) {
            m_releaseAfterPending = true;
        }
        return;
    }
    releaseDiscoveryAfterSerialization();
}

void BluetoothAppletController::releaseDiscoveryAfterSerialization()
{
    if (!m_discoveryLease.has_value()) {
        m_releaseAfterPending = false;
        m_automaticReleaseBlocked = false;
        return;
    }
    if (!presentationOwnerAvailable()
        || m_client->owner() != m_discoveryLeaseOwner) {
        // A lost/replaced owner cannot retain a caller-scoped lease. A fetch
        // failure with the same owner retains the intent until truth returns.
        if (m_client->owner() != m_discoveryLeaseOwner) {
            m_discoveryLease.reset();
            m_discoveryLeaseOwner.clear();
            m_discoveryLeaseMinimumRevision = 0;
            m_releaseAfterPending = false;
            m_automaticReleaseBlocked = false;
            reproject();
        } else {
            m_releaseAfterPending = true;
        }
        return;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::ReleaseDiscovery,
        .target = *m_discoveryLease,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    m_releaseAfterPending = false;
    if (dispatch(request)) {
        m_automaticReleaseBlocked = false;
    } else {
        m_automaticReleaseBlocked = true;
        if (!request.feedback.isEmpty()) {
            publishFeedback(request.feedback);
        }
    }
}

void BluetoothAppletController::handleOperationCompleted(
    const quint64 requestId,
    const Bluetooth::OperationResult &result)
{
    if (requestId != m_requestId || !requestInFlight()) {
        return;
    }
    const Bluetooth::OperationRequest operation = m_request.operation;
    const QString initiatingOwner = m_pendingOwner;
    RequestState completed = m_client->owner() == initiatingOwner
        ? applyBluetoothResult(m_request, result)
        : observeBluetoothAuthority(m_request, false, 0);

    m_requestId = 0;
    m_pendingOwner.clear();
    m_request = completed;
    if (completed.phase == RequestPhase::Succeeded) {
        // AGENT-GUARD: A successful operation result proves acceptance, not
        // that the client's still-published initiating snapshot has converged.
        // Keep all controls fenced until same-authority snapshot truth reaches
        // the result's observed revision; otherwise stale state can dispatch
        // the same mutation a second time.
        m_successConvergence = SuccessConvergence{
            .owner = initiatingOwner,
            .epoch = result.observedEpoch,
            .minimumRevision = result.observedRevision,
        };
        if (operation.kind == Bluetooth::OperationKind::AcquireDiscovery) {
            m_discoveryLease = operation.target;
            m_discoveryLeaseOwner = initiatingOwner;
            m_discoveryLeaseMinimumRevision = result.observedRevision;
            m_automaticReleaseBlocked = false;
        } else if (operation.kind == Bluetooth::OperationKind::ReleaseDiscovery) {
            m_discoveryLease.reset();
            m_discoveryLeaseOwner.clear();
            m_discoveryLeaseMinimumRevision = 0;
            m_automaticReleaseBlocked = false;
        } else if (operation.kind == Bluetooth::OperationKind::SetAdapterPower
                   && !operation.powered && m_discoveryLease == operation.target) {
            m_discoveryLease.reset();
            m_discoveryLeaseOwner.clear();
            m_discoveryLeaseMinimumRevision = 0;
            m_automaticReleaseBlocked = false;
        }
        publishFeedback({});
    } else {
        if (operation.kind == Bluetooth::OperationKind::ReleaseDiscovery) {
            m_automaticReleaseBlocked = true;
        }
        publishFeedback(completed.feedback);
    }

    reproject();
    if ((!m_expanded || m_releaseAfterPending || m_shuttingDown)
        && !operationPending() && !m_automaticReleaseBlocked) {
        releaseDiscoveryAfterSerialization();
    }
}

void BluetoothAppletController::observeSuccessConvergence()
{
    if (!m_successConvergence.has_value()) {
        return;
    }
    const SuccessConvergence expected = *m_successConvergence;
    if (!presentationOwnerAvailable()) {
        m_successConvergence.reset();
        publishFeedback(tr(
            "The Bluetooth result is uncertain. Check current state before retrying."));
        return;
    }
    if (m_client->owner() != expected.owner) {
        m_successConvergence.reset();
        publishFeedback(tr(
            "Bluetooth authority changed. Check current state before retrying."));
        return;
    }
    const Bluetooth::Snapshot snapshot = m_client->snapshot();
    if (snapshot.epoch != expected.epoch) {
        m_successConvergence.reset();
        publishFeedback(tr(
            "Bluetooth authority changed. Check current state before retrying."));
        return;
    }
    if (snapshot.revision >= expected.minimumRevision) {
        m_successConvergence.reset();
        clearFeedback();
    }
}

void BluetoothAppletController::retireLeaseIfAuthorityEnded()
{
    if (!m_discoveryLease.has_value()) {
        return;
    }
    if (m_client->owner().isEmpty()
        || m_client->owner() != m_discoveryLeaseOwner) {
        m_discoveryLease.reset();
        m_discoveryLeaseOwner.clear();
        m_discoveryLeaseMinimumRevision = 0;
        m_releaseAfterPending = false;
        m_automaticReleaseBlocked = false;
        return;
    }
    if (!m_client->hasSnapshot()) {
        return;
    }
    const Bluetooth::Snapshot snapshot = m_client->snapshot();
    if (snapshot.epoch != m_discoveryLease->epoch) {
        m_discoveryLease.reset();
        m_discoveryLeaseOwner.clear();
        m_discoveryLeaseMinimumRevision = 0;
        m_releaseAfterPending = false;
        m_automaticReleaseBlocked = false;
        return;
    }
    const auto adapter = std::ranges::find_if(
        snapshot.adapters, [this](const Bluetooth::Adapter &candidate) {
            return candidate.handle == *m_discoveryLease;
        });
    if (adapter == snapshot.adapters.cend() || !adapter->powered
        || (snapshot.revision >= m_discoveryLeaseMinimumRevision
            && !adapter->discovering)) {
        m_discoveryLease.reset();
        m_discoveryLeaseOwner.clear();
        m_discoveryLeaseMinimumRevision = 0;
        m_releaseAfterPending = false;
        m_automaticReleaseBlocked = false;
    }
}

void BluetoothAppletController::reproject()
{
    observeSuccessConvergence();
    retireLeaseIfAuthorityEnded();
    if (requestInFlight()) {
        const bool exact = presentationOwnerAvailable()
            && m_client->owner() == m_pendingOwner;
        const quint64 epoch = exact ? m_client->snapshot().epoch : 0;
        const RequestState observed = observeBluetoothAuthority(m_request,
                                                                exact, epoch);
        if (!observed.pending()) {
            m_request = observed;
            m_requestId = 0;
            m_pendingOwner.clear();
            publishFeedback(observed.feedback);
        }
    }

    if (!presentationOwnerAvailable()) {
        m_model = {};
        m_model.summaryLabel = tr("Bluetooth");
        if (!m_bluetoothReadGranted) {
            m_model.phase = ServicePhase::Unavailable;
            m_model.diagnostic = tr("Bluetooth access was not granted to this applet.");
        } else if (m_client->state() == Bluetooth::ClientState::Stopped
                   || m_client->state() == Bluetooth::ClientState::Starting) {
            m_model.phase = ServicePhase::Loading;
            m_model.diagnostic = tr("Bluetooth information is loading.");
        } else {
            m_model.phase = ServicePhase::Unavailable;
            m_model.diagnostic = tr("Bluetooth information is unavailable.");
        }
        m_model.accessibleName = m_model.phase == ServicePhase::Loading
            ? tr("Bluetooth information is loading")
            : tr("Bluetooth is unavailable");
        m_model.accessibleDescription = m_model.diagnostic;
        Q_EMIT stateChanged();
        return;
    }

    m_model = projectBluetoothApplet(m_client->snapshot(), true,
                                     m_bluetoothReadGranted,
                                     m_bluetoothControlGranted,
                                     m_discoveryLease);
    Q_EMIT stateChanged();

    if ((!m_expanded || m_releaseAfterPending || m_shuttingDown)
        && !operationPending() && !m_automaticReleaseBlocked
        && m_discoveryLease.has_value()) {
        releaseDiscoveryAfterSerialization();
    }
}

void BluetoothAppletController::prepareForShutdown()
{
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    setExpanded(false);
}

} // namespace QindaQt::Shell::BluetoothApplet
