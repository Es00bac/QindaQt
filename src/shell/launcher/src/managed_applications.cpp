// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/managed_applications.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QSet>
#include <QUuid>

namespace QindaQt::ShellLauncher {
namespace {

bool validText(const QString &text, int maximumBytes, bool required = false)
{
  if (text.size() > maximumBytes || (required && text.trimmed().isEmpty()))
    return false;
  for (qsizetype i = 0; i < text.size(); ++i) {
    const QChar character = text.at(i);
    if (character.category() == QChar::Other_Control)
      return false;
    if (character.isHighSurrogate()) {
      if (++i >= text.size() || !text.at(i).isLowSurrogate())
        return false;
    } else if (character.isLowSurrogate()) {
      return false;
    }
  }
  return text.toUtf8().size() <= maximumBytes;
}

bool validList(const QStringList &values, int maximumItems, int maximumBytes,
               bool categoryTokens)
{
  if (values.size() > maximumItems)
    return false;
  QSet<QString> seen;
  for (const auto &value : values) {
    if (!validText(value, maximumBytes, true) || seen.contains(value))
      return false;
    if (categoryTokens) {
      for (const QChar character : value) {
        const ushort c = character.unicode();
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '-'))
          return false;
      }
    }
    seen.insert(value);
  }
  return true;
}

} // namespace

bool isValidManagedApplicationBinding(const ManagedApplicationBinding &binding)
{
  if ((binding.origin != ApplicationOrigin::Android
       && binding.origin != ApplicationOrigin::Windows)
      || binding.registrationRevision == 0 || binding.registrationId.size() != 36
      || binding.recipeDigest.size() != 64)
    return false;
  const QUuid uuid = QUuid::fromString(binding.registrationId);
  if (uuid.isNull()
      || uuid.toString(QUuid::WithoutBraces) != binding.registrationId)
    return false;
  for (const QChar character : binding.recipeDigest) {
    const ushort c = character.unicode();
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return true;
}

QString managedApplicationEntryId(const ManagedApplicationBinding &binding)
{
  if (!isValidManagedApplicationBinding(binding))
    return {};
  const QString platform = binding.origin == ApplicationOrigin::Android
      ? QStringLiteral("android") : QStringLiteral("windows");
  return QStringLiteral("qindaqt.managed.%1.%2").arg(platform, binding.registrationId);
}

bool validateManagedApplications(
    const QVector<ManagedApplicationRegistration> &registrations, QString *error)
{
  if (error)
    error->clear();
  const auto refuse = [error](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  if (registrations.size() > Bounds::maxVisibleEntries)
    return refuse(QStringLiteral("managed application snapshot exceeds its row ceiling"));
  QSet<QString> seen;
  for (const auto &registration : registrations) {
    if (!isValidManagedApplicationBinding(registration.binding))
      return refuse(QStringLiteral("managed application binding is invalid"));
    const QString id = managedApplicationEntryId(registration.binding);
    if (seen.contains(id))
      return refuse(QStringLiteral("managed application snapshot repeats an identity"));
    seen.insert(id);
    if (!validText(registration.name, Bounds::maxNameLength, true)
        || !validText(registration.genericName, Bounds::maxGenericNameLength)
        || !validText(registration.comment, Bounds::maxCommentLength)
        || !validText(registration.iconName, Bounds::maxIconNameLength)
        || !validList(registration.categories, Bounds::maxCategories,
                      Bounds::maxCategoryLength, true)
        || !validList(registration.keywords, Bounds::maxKeywords,
                      Bounds::maxKeywordLength, false))
      return refuse(QStringLiteral("managed application display metadata is invalid"));
  }
  return true;
}

} // namespace QindaQt::ShellLauncher
