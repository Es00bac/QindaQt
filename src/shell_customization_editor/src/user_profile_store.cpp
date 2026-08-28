// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QSaveFile>

#include <utility>

namespace QindaQt::ShellCustomizationEditor {

namespace {

ProfileStoreResult failure(ProfileStoreErrorCode code, QString message)
{
    return {code, std::move(message), {}};
}

} // namespace

UserProfileStore::UserProfileStore(QString directory)
    : m_directory(std::move(directory))
{
}

QString UserProfileStore::fileNameForId(const QString &profileId)
{
    return profileId + QLatin1String(".json");
}

bool UserProfileStore::isValidProfileId(const QString &profileId)
{
    // Leading dots would create hidden files or traversal attempts (".."), so
    // identifiers must start with an alphanumeric character.
    if (profileId.isEmpty() || profileId.front().isLetterOrNumber() == false) {
        return false;
    }
    for (const QChar &character : profileId) {
        const char16_t code = character.unicode();
        const bool allowed = (code >= u'a' && code <= u'z')
            || (code >= u'A' && code <= u'Z')
            || (code >= u'0' && code <= u'9')
            || code == u'.' || code == u'_' || code == u'-';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

ProfileStoreResult UserProfileStore::save(const Profiles::LayoutProfile &profile) const
{
    if (!isValidProfileId(profile.id)) {
        return failure(ProfileStoreErrorCode::InvalidProfileId,
                       QStringLiteral("profile id '%1' cannot be used as a file name")
                           .arg(profile.id));
    }

    const QString path = m_directory + QLatin1Char('/') + fileNameForId(profile.id);

    const QJsonDocument document{profile.toJson()};
    if (!document.isObject()) {
        return failure(ProfileStoreErrorCode::SerializationFailed,
                       QStringLiteral("profile '%1' could not be serialized").arg(profile.id));
    }
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);

    const QDir directory{m_directory};
    if (!directory.exists() && !directory.mkpath(QLatin1String("."))) {
        return failure(ProfileStoreErrorCode::DirectoryUnavailable,
                       QStringLiteral("user profile directory '%1' is unavailable")
                           .arg(m_directory));
    }

    // AGENT-NOTE: QSaveFile writes a temporary file and renames it on commit,
    // so an interrupted apply leaves the previous complete document intact.
    // Direct-write fallback must stay disabled; partial bytes would break the
    // restart-safety contract on the class.
    QSaveFile file{path};
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return failure(ProfileStoreErrorCode::WriteFailed,
                       QStringLiteral("cannot open '%1' for writing: %2")
                           .arg(path, file.errorString()));
    }
    if (file.write(bytes) != bytes.size()) {
        return failure(ProfileStoreErrorCode::WriteFailed,
                       QStringLiteral("short write to '%1': %2")
                           .arg(path, file.errorString()));
    }
    if (!file.commit()) {
        return failure(ProfileStoreErrorCode::CommitFailed,
                       QStringLiteral("cannot commit '%1': %2")
                           .arg(path, file.errorString()));
    }

    ProfileStoreResult result;
    result.path = path;
    return result;
}

} // namespace QindaQt::ShellCustomizationEditor
