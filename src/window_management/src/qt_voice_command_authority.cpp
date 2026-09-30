// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <limits>
#include <qindaqt/services/voice_preferences/voice_input_preference_gate.h>
#include <qindaqt/services/voice_protocol/voice_types.h>
#include <qindaqt/window_management/qt_voice_command_authority.h>
#include <utility>
namespace QindaQt::WindowManagement {
QtVoiceCommandAuthority::QtVoiceCommandAuthority(
    Services::VoicePreferences::VoiceInputPreferenceGate &gate,
    QDBusConnection connection, std::function<bool()> locked, QObject *parent)
    : QObject(parent), m_gate(gate), m_connection(std::move(connection)),
      m_locked(std::move(locked)),
      m_watcher(QString::fromLatin1(Services::Voice::kServiceName),
                m_connection, QDBusServiceWatcher::WatchForOwnerChange, this) {
  connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this] { Q_EMIT invalidated(); });
  connect(&m_gate,
          &Services::VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
          this, [this] { Q_EMIT invalidated(); });
}
QString QtVoiceCommandAuthority::currentOwner() const {
  if (!m_gate.allowed() || !m_connection.isConnected() ||
      !m_connection.interface())
    return {};
  const QDBusReply<QString> owner = m_connection.interface()->serviceOwner(
      QString::fromLatin1(Services::Voice::kServiceName));
  return owner.isValid() && owner.value().startsWith(u':') ? owner.value()
                                                           : QString{};
}
std::optional<qint64> QtVoiceCommandAuthority::providerProcessId() const {
  const auto owner = currentOwner();
  if (owner.isEmpty())
    return std::nullopt;
  const QDBusReply<uint> pid = m_connection.interface()->servicePid(owner);
  if (!pid.isValid() || pid.value() == 0 ||
      pid.value() > uint(std::numeric_limits<int>::max()))
    return std::nullopt;
  return qint64(pid.value());
}
bool QtVoiceCommandAuthority::authorized(const QString &owner) const {
  return !owner.isEmpty() && owner == currentOwner() &&
         providerProcessId().has_value();
}
bool QtVoiceCommandAuthority::locked() const { return !m_locked || m_locked(); }
} // namespace QindaQt::WindowManagement
