// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/user_profile_store.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/profiles/profile_validation.h"

#include <QDir>
#include <QIODevice>
#include <QJsonDocument>
#include <QSaveFile>

#include <utility>

namespace QindaQt::Profiles {
namespace {

UserProfileStoreResult failure(UserProfileStoreErrorCode code, QString message)
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
    if (profileId.isEmpty() || !profileId.front().isLetterOrNumber()) {
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

UserProfileStoreResult UserProfileStore::save(const LayoutProfile &profile) const
{
    if (profile.id.isEmpty()) {
        return failure(UserProfileStoreErrorCode::EmptyProfileId,
                       QStringLiteral("profile id must not be empty"));
    }
    if (!isValidProfileId(profile.id)) {
        return failure(UserProfileStoreErrorCode::InvalidProfileId,
                       QStringLiteral("profile id '%1' cannot be used as a file name")
                           .arg(profile.id));
    }
    if (m_directory.trimmed().isEmpty()) {
        // AGENT-GUARD: QDir("").filePath() resolves relative to the process
        // current directory. Persistence must never redirect there silently.
        return failure(UserProfileStoreErrorCode::DirectoryUnavailable,
                       QStringLiteral("the user profile directory is empty"));
    }

    const ProfileValidationResult validation = ProfileValidator::validate(profile);
    if (!validation.succeeded()) {
        return failure(UserProfileStoreErrorCode::InvalidProfile,
                       validation.error.diagnostic());
    }

    const QJsonDocument document{profile.toJson()};
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    const LoadResult roundTrip =
        ProfileLoader::fromJson(bytes, QStringLiteral("user profile store preflight"));
    if (!roundTrip.ok || roundTrip.profile.toJson() != profile.toJson()) {
        const QString detail = roundTrip.ok
            ? QStringLiteral("strict loading changed the typed profile")
            : roundTrip.error.diagnostic();
        return failure(UserProfileStoreErrorCode::SerializationFailed,
                       QStringLiteral("profile '%1' failed its strict round trip: %2")
                           .arg(profile.id, detail));
    }

    QDir directory{m_directory};
    if (!directory.exists() && !directory.mkpath(QLatin1String("."))) {
        return failure(UserProfileStoreErrorCode::DirectoryUnavailable,
                       QStringLiteral("user profile directory '%1' is unavailable")
                           .arg(m_directory));
    }
    if (!directory.exists()) {
        return failure(UserProfileStoreErrorCode::DirectoryUnavailable,
                       QStringLiteral("user profile directory '%1' is unavailable")
                           .arg(m_directory));
    }
    const QString path = directory.filePath(fileNameForId(profile.id));

    // AGENT-NOTE: direct-write fallback stays disabled. A failed or interrupted
    // write must leave either the prior complete document or no document.
    QSaveFile file{path};
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return failure(UserProfileStoreErrorCode::WriteFailed,
                       QStringLiteral("cannot open '%1' for writing: %2")
                           .arg(path, file.errorString()));
    }
    if (file.write(bytes) != bytes.size()) {
        return failure(UserProfileStoreErrorCode::WriteFailed,
                       QStringLiteral("short write to '%1': %2")
                           .arg(path, file.errorString()));
    }
    if (!file.commit()) {
        return failure(UserProfileStoreErrorCode::CommitFailed,
                       QStringLiteral("cannot commit '%1': %2")
                           .arg(path, file.errorString()));
    }

    UserProfileStoreResult result;
    result.path = path;
    return result;
}

} // namespace QindaQt::Profiles
