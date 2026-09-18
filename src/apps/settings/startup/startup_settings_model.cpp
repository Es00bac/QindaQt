// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_startup/startup_settings_model.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsStartup {

StartupSettingsModel::StartupSettingsModel(std::unique_ptr<AutostartStore> store,
                                           QObject *parent)
    : QObject(parent), m_store(std::move(store)) {
  refresh();
}

QVariantMap
StartupSettingsModel::projectEntry(const AutostartEntry &entry) const {
  return QVariantMap{
      {QStringLiteral("id"), entry.id},
      {QStringLiteral("name"), entry.name},
      {QStringLiteral("comment"), entry.comment},
      {QStringLiteral("iconName"), entry.iconName},
      {QStringLiteral("exec"), entry.exec},
      {QStringLiteral("enabled"), entry.enabled},
      {QStringLiteral("custom"), entry.custom},
  };
}

QVariantList StartupSettingsModel::entries() const {
  QVariantList rows;
  rows.reserve(m_entries.size());
  for (const AutostartEntry &entry : m_entries) {
    rows.append(projectEntry(entry));
  }
  return rows;
}

void StartupSettingsModel::refresh() {
  QString error;
  m_entries = m_store->list(&error);
  m_errorText = error;
  Q_EMIT viewChanged();
}

bool StartupSettingsModel::setEnabled(const QString &id, const bool enabled) {
  QString error;
  if (!m_store->setEnabled(id, enabled, &error)) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  refresh();
  return true;
}

bool StartupSettingsModel::addCommand(const QString &name,
                                      const QString &command) {
  QString error;
  const QString id = m_store->addCommand(name, command, &error);
  if (id.isEmpty()) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  refresh();
  return true;
}

bool StartupSettingsModel::removeCustom(const QString &id) {
  QString error;
  if (!m_store->removeCustom(id, &error)) {
    m_errorText = error;
    Q_EMIT viewChanged();
    return false;
  }
  refresh();
  return true;
}

} // namespace QindaQt::Apps::SettingsStartup
