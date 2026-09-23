// SPDX-License-Identifier: GPL-3.0-or-later
#include "week_start_preference.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_transport.h"

#include <QVariant>

#include <utility>

namespace QindaQt::Apps::SettingsDateTime {
namespace {

// AGENT-GUARD: this adapter must remain scoped to only the key Calendar reads.
constexpr auto weekStartKey = "services.calendarWeekStart";
using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsClient::SettingsClient;
using Services::SettingsClient::SettingsTransport;
using Services::SettingsProtocol::SettingsWireStatus;

[[nodiscard]] bool validWeekStart(const QString &value) {
  return value == QLatin1String("locale") || value == QLatin1String("monday") ||
         value == QLatin1String("sunday");
}

[[nodiscard]] QString normalize(const QString &value) {
  return validWeekStart(value) ? value : QStringLiteral("locale");
}

} // namespace

SettingsWeekStartPreference::SettingsWeekStartPreference(QDBusConnection bus,
                                                         QObject *parent)
    : SettingsWeekStartPreference(
          std::make_unique<Services::SettingsClient::QtSettingsTransport>(
              std::move(bus)), {}, parent) {}

SettingsWeekStartPreference::SettingsWeekStartPreference(
    std::unique_ptr<SettingsTransport> transport,
    Services::SettingsClient::ClientTiming timing, QObject *parent)
    : WeekStartPreference(parent), m_transport(std::move(transport)),
      m_client(std::make_unique<SettingsClient>(
          *m_transport, QStringList{QLatin1StringView(weekStartKey)}, timing)) {
  m_readbackTimeout.setSingleShot(true);
  m_readbackTimeout.setInterval(2 * timing.requestTimeoutMilliseconds + 100);
  connect(&m_readbackTimeout, &QTimer::timeout, this, [this] {
    if (m_writeState == WeekStartWriteState::Pending && m_awaitingReadback) {
      markUncertain(tr("Settings accepted the change, but a fresh value could not be confirmed."));
      m_client->refresh();
    }
  });
  connect(m_client.get(), &SettingsClient::snapshotChanged, this,
          &SettingsWeekStartPreference::applySnapshot);
  connect(m_client.get(), &SettingsClient::stateChanged, this,
          &SettingsWeekStartPreference::handleClientState);
  // The owner may change Authenticating-to-Authenticating without stateChanged.
  connect(m_client.get(), &SettingsClient::ownerChanged, this,
          &SettingsWeekStartPreference::handleClientState);
  connect(m_client.get(), &SettingsClient::writeAdmissionChanged, this,
          &SettingsWeekStartPreference::weekStartChanged);
  connect(m_client.get(), &SettingsClient::commitFinished, this,
          &SettingsWeekStartPreference::handleCommit);
  connect(m_client.get(), &SettingsClient::commitUncertain, this,
          &SettingsWeekStartPreference::markUncertain);
  QString error;
  if (m_client->start(&error)) {
    applySnapshot();
  } else {
    Q_EMIT weekStartChanged();
  }
}

SettingsWeekStartPreference::~SettingsWeekStartPreference() = default;

QString SettingsWeekStartPreference::weekStart() const { return m_weekStart; }

bool SettingsWeekStartPreference::editable() const {
  return m_writeState != WeekStartWriteState::Pending &&
         m_client->canSetUserValue(QLatin1StringView(weekStartKey));
}

WeekStartWriteState SettingsWeekStartPreference::writeState() const {
  return m_writeState;
}

QString SettingsWeekStartPreference::diagnostic() const { return m_diagnostic; }

QString SettingsWeekStartPreference::availabilityText() const {
  if (editable()) {
    return {};
  }
  if (m_client->state() == ClientState::Ready) {
    return tr("Refreshing the calendar preference…");
  }
  const QString reason = m_client->lastError().left(512);
  return reason.isEmpty()
             ? tr("The calendar preference is unavailable until Settings reconnects.")
             : tr("The calendar preference is unavailable: %1").arg(reason);
}

void SettingsWeekStartPreference::applySnapshot() {
  const auto &snapshot = m_client->snapshot();
  const bool confirmed = m_client->state() == ClientState::Ready && snapshot &&
                         snapshot->owner == m_client->currentOwner();
  if (confirmed) {
    m_weekStart = normalize(
        snapshot->values.value(QLatin1String(weekStartKey)).toString());
    if (m_writeState == WeekStartWriteState::Pending && m_awaitingReadback) {
      if (snapshot->owner != m_writeOwner || snapshot->epoch != m_writeEpoch) {
        markUncertain(tr("Settings changed before the calendar preference could be confirmed."));
      } else if (snapshot->revision >= m_revisionFloor) {
        m_readbackTimeout.stop();
        m_awaitingReadback = false;
        if (m_weekStart == m_requested) {
          m_writeState = WeekStartWriteState::Idle;
          m_diagnostic.clear();
        } else {
          m_writeState = WeekStartWriteState::Conflict;
          m_diagnostic = tr("The calendar preference changed elsewhere before confirmation.");
        }
      }
      // A same-lineage snapshot older than the Applied revision is stale;
      // keep the request pending until a newer snapshot or bounded timeout.
    }
  }
  Q_EMIT weekStartChanged();
}

void SettingsWeekStartPreference::handleClientState() {
  if (m_writeState == WeekStartWriteState::Pending &&
      (m_client->currentOwner() != m_writeOwner ||
       m_client->state() == ClientState::Unavailable ||
       m_client->state() == ClientState::Degraded)) {
    markUncertain(tr("Settings changed before the calendar preference could be confirmed."));
  }
  Q_EMIT weekStartChanged();
}

void SettingsWeekStartPreference::handleCommit(const CommitOutcome &outcome) {
  if (m_writeState != WeekStartWriteState::Pending) {
    return;
  }
  if (outcome.status == SettingsWireStatus::Applied) {
    m_revisionFloor = outcome.revisionAfter;
    m_awaitingReadback = true;
    m_readbackTimeout.start();
  } else {
    m_readbackTimeout.stop();
    m_awaitingReadback = false;
    m_writeState = outcome.status == SettingsWireStatus::Conflict ||
                           outcome.status == SettingsWireStatus::EpochMismatch
                       ? WeekStartWriteState::Conflict
                       : WeekStartWriteState::Refused;
    const QString reason = outcome.message.isEmpty()
                               ? Services::SettingsProtocol::settingsWireStatusName(outcome.status)
                               : outcome.message.left(512);
    m_diagnostic = m_writeState == WeekStartWriteState::Conflict
                       ? tr("The calendar preference changed elsewhere: %1").arg(reason)
                       : tr("The calendar preference was refused: %1").arg(reason);
  }
  Q_EMIT weekStartChanged();
}

void SettingsWeekStartPreference::markUncertain(const QString &message) {
  if (m_writeState != WeekStartWriteState::Pending) {
    return;
  }
  m_readbackTimeout.stop();
  m_awaitingReadback = false;
  m_writeState = WeekStartWriteState::Uncertain;
  m_diagnostic = tr("The calendar preference may have changed. %1 Refresh to see the current value; no write was retried.")
                     .arg(message.left(512));
  Q_EMIT weekStartChanged();
}

bool SettingsWeekStartPreference::setWeekStart(const QString &weekStart) {
  if (!validWeekStart(weekStart) || !editable()) {
    m_writeState = WeekStartWriteState::Refused;
    m_diagnostic = tr("The calendar preference is not ready for this change.");
    Q_EMIT weekStartChanged();
    return false;
  }
  const auto &snapshot = m_client->snapshot();
  m_requested = weekStart;
  m_writeOwner = snapshot->owner;
  m_writeEpoch = snapshot->epoch;
  m_revisionFloor = 0;
  m_awaitingReadback = false;
  m_writeState = WeekStartWriteState::Pending;
  m_diagnostic.clear();
  Q_EMIT weekStartChanged();
  QString error;
  if (!m_client->setUserValue(QLatin1StringView(weekStartKey), weekStart,
                              &error)) {
    m_writeState = WeekStartWriteState::Refused;
    m_diagnostic = error.isEmpty()
                       ? tr("The calendar preference could not be submitted.")
                       : error.left(512);
    Q_EMIT weekStartChanged();
    return false;
  }
  return true;
}

void SettingsWeekStartPreference::refresh() { m_client->refresh(); }

} // namespace QindaQt::Apps::SettingsDateTime
