// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}
namespace QindaQt::Services::LockPreferences {
struct Preferences final {
  bool automaticLock = true;
  qint64 idleTimeoutSeconds = 300;
  bool lockOnResume = true;
  qint64 graceSeconds = 5;
  bool operator==(const Preferences &) const = default;
};
QStringList scopedKeys();
// Exact typed Settings1 values only; invalid or incomplete snapshots leave
// output unchanged.
bool decodePreferences(const QVariantMap &values, Preferences *output);
// Borrowed same-thread client and captures must outlive the provider. No
// storage, lock request, timer or QML authority; consumers choose their policy
// explicitly.
class PreferencesProvider final : public QObject {
  Q_OBJECT
public:
  explicit PreferencesProvider(SettingsClient::SettingsClient &borrowed,
                               QObject *parent = nullptr);
  std::optional<Preferences> preferences() const;
Q_SIGNALS:
  void changed();

private:
  SettingsClient::SettingsClient &m_client;
};
} // namespace QindaQt::Services::LockPreferences
