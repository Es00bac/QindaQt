// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/lock_preferences/lock_preferences.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include <QMetaType>
namespace QindaQt::Services::LockPreferences {
namespace {
bool integer(const QVariant &value, qint64 minimum, qint64 maximum,
             qint64 *output) {
  const int type = value.metaType().id();
  if (type != QMetaType::Int && type != QMetaType::UInt &&
      type != QMetaType::LongLong && type != QMetaType::ULongLong)
    return false;
  bool ok = false;
  const auto number = value.toLongLong(&ok);
  if (!ok || number < minimum || number > maximum)
    return false;
  *output = number;
  return true;
}
} // namespace
QStringList scopedKeys() {
  return {QStringLiteral("lock.automaticEnabled"),
          QStringLiteral("lock.idleTimeoutSeconds"),
          QStringLiteral("lock.onResume"), QStringLiteral("lock.graceSeconds")};
}
bool decodePreferences(const QVariantMap &values, Preferences *output) {
  if (!output)
    return false;
  Preferences next;
  const auto automatic = values.value(QStringLiteral("lock.automaticEnabled"));
  const auto resume = values.value(QStringLiteral("lock.onResume"));
  if (automatic.metaType() != QMetaType::fromType<bool>() ||
      resume.metaType() != QMetaType::fromType<bool>() ||
      !integer(values.value(QStringLiteral("lock.idleTimeoutSeconds")), 60,
               14400, &next.idleTimeoutSeconds) ||
      !integer(values.value(QStringLiteral("lock.graceSeconds")), 0, 300,
               &next.graceSeconds))
    return false;
  next.automaticLock = automatic.toBool();
  next.lockOnResume = resume.toBool();
  *output = next;
  return true;
}
PreferencesProvider::PreferencesProvider(SettingsClient::SettingsClient &client,
                                         QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&client, &SettingsClient::SettingsClient::stateChanged, this,
          &PreferencesProvider::changed);
  connect(&client, &SettingsClient::SettingsClient::ownerChanged, this,
          &PreferencesProvider::changed);
  connect(&client, &SettingsClient::SettingsClient::snapshotChanged, this,
          &PreferencesProvider::changed);
}
std::optional<Preferences> PreferencesProvider::preferences() const {
  const auto &snapshot = m_client.snapshot();
  Preferences next;
  if (m_client.state() != SettingsClient::ClientState::Ready || !snapshot ||
      snapshot->owner.isEmpty() || snapshot->owner != m_client.currentOwner() ||
      snapshot->epoch.isEmpty() || !decodePreferences(snapshot->values, &next))
    return std::nullopt;
  return next;
}
} // namespace QindaQt::Services::LockPreferences
