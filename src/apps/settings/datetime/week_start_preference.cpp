// SPDX-License-Identifier: GPL-3.0-or-later
#include "week_start_preference.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QVariant>

#include <utility>

namespace QindaQt::Apps::SettingsDateTime {
namespace {

// AGENT-GUARD: one key, and only this key. See the header's note on scoping.
constexpr auto weekStartKey = "services.calendarWeekStart";

[[nodiscard]] QString normalize(const QString &value) {
  if (value == QLatin1String("monday") || value == QLatin1String("sunday")) {
    return value;
  }
  return QStringLiteral("locale");
}

} // namespace

SettingsWeekStartPreference::SettingsWeekStartPreference(QDBusConnection bus,
                                                         QObject *parent)
    : WeekStartPreference(parent),
      m_transport(
          std::make_unique<QindaQt::Services::SettingsClient::QtSettingsTransport>(
              std::move(bus))),
      m_client(std::make_unique<QindaQt::Services::SettingsClient::SettingsClient>(
          *m_transport, QStringList{QLatin1StringView(weekStartKey)})) {
  connect(m_client.get(),
          &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &SettingsWeekStartPreference::applySnapshot);
  connect(m_client.get(),
          &QindaQt::Services::SettingsClient::SettingsClient::stateChanged, this,
          &SettingsWeekStartPreference::applySnapshot);
  QString error;
  // A settings service that will not start leaves the control unavailable
  // rather than pretending the schema default is the user's choice.
  if (m_client->start(&error)) {
    applySnapshot();
  }
}

SettingsWeekStartPreference::~SettingsWeekStartPreference() = default;

QString SettingsWeekStartPreference::weekStart() const { return m_weekStart; }

bool SettingsWeekStartPreference::editable() const { return m_ready; }

void SettingsWeekStartPreference::applySnapshot() {
  const auto &snapshot = m_client->snapshot();
  const bool ready =
      snapshot.has_value() &&
      m_client->state() != QindaQt::Services::SettingsClient::ClientState::Unavailable;
  const QString next =
      ready ? normalize(snapshot->values.value(QLatin1String(weekStartKey)).toString())
            : m_weekStart;
  if (ready == m_ready && next == m_weekStart) {
    return;
  }
  m_ready = ready;
  m_weekStart = next;
  Q_EMIT weekStartChanged();
}

void SettingsWeekStartPreference::setWeekStart(const QString &weekStart) {
  if (!m_ready) {
    return;
  }
  QString error;
  // The published value follows the service's own snapshot, never the request:
  // a refused write must not look like a change.
  const bool accepted =
      m_client->setUserValue(QLatin1StringView(weekStartKey), normalize(weekStart),
                             &error);
  Q_UNUSED(accepted);
}

} // namespace QindaQt::Apps::SettingsDateTime
