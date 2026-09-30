// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <qindaqt/window_management/controller.h>
namespace QindaQt::Services::VoicePreferences {
class VoiceInputPreferenceGate;
}
namespace QindaQt::WindowManagement {
// Borrows the confirmed Settings1 gate and lock predicate. All dependencies
// outlive this GUI-thread object. Bus-daemon credentials and live Voice1 owner
// are queried again at admission, never supplied by the command provider.
class QtVoiceCommandAuthority final : public QObject, public Authority {
  Q_OBJECT
public:
  QtVoiceCommandAuthority(
      Services::VoicePreferences::VoiceInputPreferenceGate &gate,
      QDBusConnection connection, std::function<bool()> locked,
      QObject *parent = nullptr);
  bool authorized(const QString &uniqueOwner) const override;
  bool locked() const override;
  [[nodiscard]] std::optional<qint64> providerProcessId() const;
Q_SIGNALS:
  void invalidated();

private:
  QString currentOwner() const;
  Services::VoicePreferences::VoiceInputPreferenceGate &m_gate;
  QDBusConnection m_connection;
  std::function<bool()> m_locked;
  QDBusServiceWatcher m_watcher;
};
} // namespace QindaQt::WindowManagement
