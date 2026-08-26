// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_loader.h"

#include "profile_json_reader_p.h"
#include "profile_json_syntax_p.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include <utility>

namespace QindaQt::Profiles {
namespace {

LoadResult failure(ProfileError error)
{
    return {.ok = false, .profile = {}, .error = std::move(error)};
}

ProfileError parseFailure(const QString &origin,
                          QString message,
                          qsizetype byteOffset = -1)
{
    return {.code = ProfileErrorCode::InvalidJson,
            .origin = origin,
            .path = {},
            .panelId = {},
            .appletId = {},
            .message = std::move(message),
            .byteOffset = byteOffset};
}

bool rootStartsWithObject(const QByteArray &json)
{
    qsizetype position = 0;
    while (position < json.size()) {
        const char byte = json.at(position);
        if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
            return byte == '{';
        }
        ++position;
    }
    return false;
}

} // namespace

LoadResult ProfileLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure({.code = ProfileErrorCode::FileReadFailed,
                        .origin = path,
                        .path = {},
                        .panelId = {},
                        .appletId = {},
                        .message = file.errorString(),
                        .byteOffset = -1});
    }
    const QByteArray json = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return failure({.code = ProfileErrorCode::FileReadFailed,
                        .origin = path,
                        .path = {},
                        .panelId = {},
                        .appletId = {},
                        .message = file.errorString(),
                        .byteOffset = -1});
    }
    return fromJson(json, path);
}

LoadResult ProfileLoader::fromJson(const QByteArray &json, const QString &origin)
{
    const ProfileError syntaxError = Internal::validateJsonSyntax(json, origin);
    if (syntaxError.hasError()) {
        return failure(syntaxError);
    }
    if (!rootStartsWithObject(json)) {
        return failure({.code = ProfileErrorCode::InvalidRoot,
                        .origin = origin,
                        .path = {},
                        .panelId = {},
                        .appletId = {},
                        .message = QStringLiteral("profile root must be an object"),
                        .byteOffset = -1});
    }

    QJsonParseError qtError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &qtError);
    if (qtError.error != QJsonParseError::NoError) {
        return failure(parseFailure(origin, qtError.errorString(), qtError.offset));
    }
    if (!document.isObject()) {
        return failure({.code = ProfileErrorCode::InvalidRoot,
                        .origin = origin,
                        .path = {},
                        .panelId = {},
                        .appletId = {},
                        .message = QStringLiteral("profile root must be an object"),
                        .byteOffset = -1});
    }

    Internal::ProfileJsonReadResult read =
        Internal::readProfileObject(document.object(), origin);
    if (!read.succeeded()) {
        return failure(std::move(read.error));
    }

    ProfileValidationResult validation = ProfileValidator::validate(read.profile);
    if (!validation.succeeded()) {
        validation.error.origin = origin;
        return failure(std::move(validation.error));
    }
    return {.ok = true, .profile = std::move(read.profile), .error = {}};
}

QVector<LoadResult> ProfileLoader::fromDirectory(const QString &path)
{
    QDir directory(path);
    const auto names = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVector<LoadResult> results;
    results.reserve(names.size());
    for (const auto &name : names) {
        results.append(fromFile(directory.filePath(name)));
    }
    return results;
}

} // namespace QindaQt::Profiles
