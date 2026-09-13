// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>

#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QMetaType>

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::DisplayClient {

void Client::initializeBrightness() {
  qRegisterMetaType<Display::BrightnessSnapshot>();
  m_brightnessTimer.setSingleShot(true);
  connect(m_transport, &DisplayTransport::brightnessReply, this,
          &Client::acceptBrightnessReply);
  connect(&m_brightnessTimer, &QTimer::timeout, this, [this]() {
    if (m_state == ClientState::Stopped || !m_brightnessFetchInFlight) {
      return;
    }
    m_brightnessFetchInFlight = false;
    scheduleRefetch();
  });
}

std::optional<Display::BrightnessSnapshot> Client::brightness() const {
  return m_brightness;
}

quint64 Client::setOutputBrightness(const Display::BrightnessRequest &request) {
  if (m_state == ClientState::Stopped) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("client-not-running"));
  }
  if (operationPending()) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("operation-pending"));
  }
  if (!m_snapshot.has_value() || !m_brightness.has_value()) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("no-snapshot"));
  }
  if (!Display::validateBrightnessRequest(request).accepted) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("invalid-brightness-request"));
  }
  // AGENT-GUARD: the request must name the exact published brightness
  // lineage and exactly one joined stable output. A value chosen against
  // older, foreign, or unplugged truth never reaches the transport.
  if (request.baseEpoch != m_brightness->serviceEpoch ||
      request.baseRevision != m_brightness->revision) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("stale-revision"));
  }
  if (std::ranges::count(m_brightness->outputs, request.stableId,
                         &Display::OutputBrightness::stableId) != 1) {
    return rejectLocally(OperationKind::Brightness,
                         QStringLiteral("unknown-output"));
  }

  const quint64 requestId = beginOperation(OperationKind::Brightness, {});
  if (requestId == 0) {
    return 0;
  }
  // Immediate results name the brightness revision namespace (ADR-0150), not
  // the topology revision beginOperation() recorded.
  m_operation->epochAtSubmit = request.baseEpoch;
  m_operation->revisionAtSubmit = request.baseRevision;
  m_transport->submitOutputBrightness(m_owner, requestId, request);
  return requestId;
}

void Client::requestBrightness() {
  if (m_state == ClientState::Stopped || m_owner.isEmpty() ||
      !m_snapshot.has_value()) {
    return;
  }
  if (m_brightnessFetchInFlight) {
    m_brightnessRefetchNeeded = true;
    return;
  }
  if (m_nextRequestId == 0 ||
      m_nextRequestId == std::numeric_limits<quint64>::max()) {
    return;
  }
  m_brightnessFetchInFlight = true;
  m_brightnessRefetchNeeded = false;
  m_brightnessRequestId = m_nextRequestId++;
  m_brightnessTimer.start(m_requestTimeoutMs);
  m_transport->fetchBrightness(m_owner, m_brightnessRequestId);
}

void Client::clearBrightness() {
  m_brightnessFetchInFlight = false;
  m_brightnessRefetchNeeded = false;
  m_brightnessTimer.stop();
  if (m_brightness.has_value()) {
    m_brightness.reset();
    Q_EMIT brightnessChanged();
  }
}

void Client::acceptBrightnessReply(const QString &owner, quint64 requestId,
                                   bool transportSuccess,
                                   const Display::BrightnessSnapshot &brightness,
                                   const QString &reasonCode) {
  if (m_state == ClientState::Stopped || !m_brightnessFetchInFlight ||
      requestId != m_brightnessRequestId) {
    return;
  }
  m_brightnessFetchInFlight = false;
  m_brightnessTimer.stop();
  const bool refetch = std::exchange(m_brightnessRefetchNeeded, false);
  if (owner != m_owner || !m_snapshot.has_value()) {
    return;
  }
  if (!transportSuccess) {
    // A service without published brightness, or one that predates it,
    // leaves topology untouched and publishes no rows. Only a timeout is
    // worth another read.
    clearBrightness();
    if (reasonCode == QStringLiteral("transport-timeout")) {
      scheduleRefetch();
    }
    return;
  }
  if (brightness.serviceEpoch == m_snapshot->serviceEpoch &&
      brightness.topologyRevision > m_snapshot->revision) {
    // Rows for a newer topology: read that topology, then join again.
    clearBrightness();
    requestSnapshot();
    return;
  }

  // AGENT-GUARD: publish only rows that pass the public join against the held
  // snapshot (epoch, topology revision, and stable output order). Within one
  // epoch an older or same-revision hybrid reply never replaces truth.
  if (!Display::validateBrightnessJoin(*m_snapshot, brightness).accepted) {
    clearBrightness();
  } else {
    const bool regresses =
        m_brightness.has_value() &&
        m_brightness->serviceEpoch == brightness.serviceEpoch &&
        (brightness.revision < m_brightness->revision ||
         (brightness.revision == m_brightness->revision &&
          brightness != *m_brightness));
    if (!regresses && m_brightness != brightness) {
      m_brightness = brightness;
      Q_EMIT brightnessChanged();
    }
  }
  if (refetch) {
    requestBrightness();
  }
}

} // namespace QindaQt::DisplayClient
