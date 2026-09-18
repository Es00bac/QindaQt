// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_datetime/datetime_settings_model.h"

#include <utility>

namespace QindaQt::Apps::SettingsDateTime {
namespace {

constexpr int maximumDiagnosticLength = 512;

} // namespace

DateTimeSettingsModel::DateTimeSettingsModel(SystemTimeServicePtr service,
                                             WeekStartPreferencePtr weekStart,
                                             QObject *parent)
    : QObject(parent), m_service(std::move(service)),
      m_weekStart(std::move(weekStart)) {
  if (m_service) {
    connect(m_service.get(), &SystemTimeService::snapshotChanged, this,
            &DateTimeSettingsModel::onSnapshotChanged);
    connect(m_service.get(), &SystemTimeService::requestFailed, this,
            &DateTimeSettingsModel::onRequestFailed);
    m_service->refresh();
  }
  if (m_weekStart) {
    connect(m_weekStart.get(), &WeekStartPreference::weekStartChanged, this,
            &DateTimeSettingsModel::viewChanged);
  }
}

bool DateTimeSettingsModel::timeZoneEditable() const {
  // An empty catalogue means the platform could not list its zones; the page
  // then shows the current one read-only rather than an empty picker.
  return m_snapshot.available && !m_snapshot.timeZones.isEmpty();
}

QString DateTimeSettingsModel::synchronizationText() const {
  if (!m_snapshot.available) {
    return {};
  }
  if (!m_snapshot.automaticTime) {
    return tr("The clock is set manually.");
  }
  return m_snapshot.synchronized
             ? tr("The clock is synchronized with a time server.")
             : tr("Waiting for a time server.");
}

QString DateTimeSettingsModel::weekStart() const {
  return m_weekStart ? m_weekStart->weekStart() : QStringLiteral("locale");
}

QStringList DateTimeSettingsModel::weekStarts() const {
  return {QStringLiteral("locale"), QStringLiteral("monday"),
          QStringLiteral("sunday")};
}

bool DateTimeSettingsModel::weekStartEditable() const {
  return m_weekStart != nullptr && m_weekStart->editable();
}

QString DateTimeSettingsModel::statusText() const {
  if (!m_snapshot.available) {
    return tr("The system clock service is unavailable, so the date and time "
              "cannot be changed here.");
  }
  if (busy()) {
    return tr("Waiting for the system to confirm the change…");
  }
  return {};
}

void DateTimeSettingsModel::onSnapshotChanged(const SystemTimeSnapshot &snapshot) {
  const bool same = snapshot == m_snapshot;
  m_snapshot = snapshot;
  finishRequest();
  if (!same) {
    Q_EMIT viewChanged();
  }
}

void DateTimeSettingsModel::onRequestFailed(const QString &diagnostic) {
  // AGENT-GUARD: a refusal leaves every published value exactly as the
  // platform last reported it. Nothing here writes the value the user asked
  // for -- polkit saying no must not look like success.
  m_errorText = diagnostic.left(maximumDiagnosticLength);
  finishRequest();
  Q_EMIT viewChanged();
}

void DateTimeSettingsModel::finishRequest() {
  if (m_pendingRequests > 0) {
    --m_pendingRequests;
  }
}

void DateTimeSettingsModel::requestTimeZone(const QString &timeZone) {
  if (!m_service || !timeZoneEditable() || timeZone == m_snapshot.timeZone) {
    return;
  }
  // Only a zone the platform itself offered is ever requested.
  if (!m_snapshot.timeZones.contains(timeZone)) {
    return;
  }
  m_errorText.clear();
  ++m_pendingRequests;
  Q_EMIT viewChanged();
  m_service->setTimeZone(timeZone);
}

void DateTimeSettingsModel::requestAutomaticTime(const bool enabled) {
  if (!m_service || !automaticTimeSupported() ||
      enabled == m_snapshot.automaticTime) {
    return;
  }
  m_errorText.clear();
  ++m_pendingRequests;
  Q_EMIT viewChanged();
  m_service->setAutomaticTime(enabled);
}

void DateTimeSettingsModel::requestWeekStart(const QString &weekStart) {
  if (!weekStartEditable() || !weekStarts().contains(weekStart) ||
      weekStart == m_weekStart->weekStart()) {
    return;
  }
  m_weekStart->setWeekStart(weekStart);
}

void DateTimeSettingsModel::refresh() {
  if (m_service) {
    m_service->refresh();
  }
}

void DateTimeSettingsModel::clearError() {
  if (m_errorText.isEmpty()) {
    return;
  }
  m_errorText.clear();
  Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsDateTime
