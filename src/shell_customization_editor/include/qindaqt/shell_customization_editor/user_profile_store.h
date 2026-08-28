// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>

namespace QindaQt::ShellCustomizationEditor {

// Typed outcomes for user-profile persistence. Every failure is deterministic
// and leaves the previous file bytes untouched, so a crashed or cancelled
// apply can never publish a partial profile.
enum class ProfileStoreErrorCode {
    None,
    EmptyProfileId,
    InvalidProfileId,
    DirectoryUnavailable,
    SerializationFailed,
    WriteFailed,
    CommitFailed,
};

struct ProfileStoreResult final {
    ProfileStoreErrorCode code = ProfileStoreErrorCode::None;
    QString message;
    QString path;

    [[nodiscard]] bool ok() const noexcept { return code == ProfileStoreErrorCode::None; }
};

// Writes derived user layout profiles as strict schema-v1 JSON documents.
//
// AGENT-CONTRACT: this store only writes complete documents atomically under
// its directory (`<profile-id>.json`). Layered catalog precedence (user wins
// on id collision) belongs to the profiles module's catalog work; do not
// improvise shadowing rules here. Restart safety comes from the QSaveFile
// commit: either the previous bytes or the complete new bytes survive.
class UserProfileStore final {
public:
    explicit UserProfileStore(QString directory);

    // Serializes and atomically replaces `<directory>/<id>.json`.
    [[nodiscard]] ProfileStoreResult save(const Profiles::LayoutProfile &profile) const;

    [[nodiscard]] const QString &directory() const noexcept { return m_directory; }

    [[nodiscard]] static QString fileNameForId(const QString &profileId);
    // Only plain file-name-safe identifiers are accepted: the id becomes a
    // file name and must never traverse directories.
    [[nodiscard]] static bool isValidProfileId(const QString &profileId);

private:
    QString m_directory;
};

} // namespace QindaQt::ShellCustomizationEditor
