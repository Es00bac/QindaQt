// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_default_apps/default_applications_settings_model.h>

#include <qindaqt/apps/settings_default_apps/default_applications_catalog.h>

#include <QtCore/QVariantMap>

#include <utility>

namespace QindaQt::Apps::SettingsDefaultApps {
namespace {
QString candidateName(const QVector<CandidateApplication> &candidates,
                      const QString &desktopId) {
  for (const CandidateApplication &candidate : candidates) {
    if (candidate.id == desktopId) return candidate.name;
  }
  return desktopId;
}
} // namespace

DefaultApplicationsSettingsModel::DefaultApplicationsSettingsModel(
    std::unique_ptr<DefaultApplicationsStore> store,
    QindaQt::ApplicationCatalog::DirectoryScan scan, QObject *parent)
    : QObject(parent), m_store(std::move(store)), m_scan(std::move(scan)) {
  reload();
}

void DefaultApplicationsSettingsModel::reload() {
  QString error;
  if (!m_store->load(&m_preferences, &error)) {
    m_loadFailed = true;
    m_errorText = error;
    return;
  }
  m_loadFailed = false;
  m_errorText.clear();
}

QVariantList DefaultApplicationsSettingsModel::rows() const {
  QVariantList result;
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    const QVector<CandidateApplication> candidates =
        candidateApplicationsForCategory(m_scan, category);
    const QString currentId = m_preferences.category(category);
    QVariantList options;
    for (const CandidateApplication &candidate : candidates) {
      options.append(QVariantMap{
          {QStringLiteral("id"), candidate.id},
          {QStringLiteral("name"), candidate.name},
          {QStringLiteral("iconName"), candidate.iconName},
      });
    }
    result.append(QVariantMap{
        {QStringLiteral("id"), defaultApplicationCategoryId(category)},
        {QStringLiteral("label"), defaultApplicationCategoryLabel(category)},
        {QStringLiteral("currentId"), currentId},
        {QStringLiteral("currentName"),
         currentId.isEmpty() ? QString() : candidateName(candidates, currentId)},
        {QStringLiteral("options"), options},
        {QStringLiteral("accessibleDescription"),
         currentId.isEmpty()
             ? QStringLiteral("%1: no default set")
                   .arg(defaultApplicationCategoryLabel(category))
             : QStringLiteral("%1: %2")
                   .arg(defaultApplicationCategoryLabel(category),
                        candidateName(candidates, currentId))},
    });
  }
  return result;
}

bool DefaultApplicationsSettingsModel::setDefaultApplication(
    const QString &categoryId, const QString &desktopId) {
  DefaultApplicationCategory matched{};
  bool found = false;
  for (const DefaultApplicationCategory category : kDefaultApplicationCategories) {
    if (defaultApplicationCategoryId(category) == categoryId) {
      matched = category;
      found = true;
      break;
    }
  }
  if (!found) {
    m_errorText = QStringLiteral("default-applications-unknown-category");
    Q_EMIT changed();
    return false;
  }
  DefaultApplicationPreferences next = m_preferences;
  next.setCategory(matched, desktopId);
  QString error;
  if (!m_store->save(next, &error)) {
    m_errorText = error;
    Q_EMIT changed();
    return false;
  }
  m_preferences = next;
  m_loadFailed = false;
  m_errorText.clear();
  Q_EMIT changed();
  return true;
}

bool DefaultApplicationsSettingsModel::retry() {
  reload();
  Q_EMIT changed();
  return !m_loadFailed;
}

} // namespace QindaQt::Apps::SettingsDefaultApps
