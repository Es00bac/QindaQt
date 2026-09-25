// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>

namespace QindaQt::Profiles {

enum class UserProfileStoreErrorCode {
    None,
    EmptyProfileId,
    InvalidProfileId,
    InvalidProfile,
    DirectoryUnavailable,
    SerializationFailed,
    WriteFailed,
    CommitFailed,
    NotFound,
    RemoveFailed,
};

struct UserProfileStoreResult final {
    UserProfileStoreErrorCode code = UserProfileStoreErrorCode::None;
    QString message;
    QString path;

    [[nodiscard]] bool ok() const noexcept
    {
        return code == UserProfileStoreErrorCode::None;
    }
};

// Sole schema-v1 layout-profile file writer. Presentation and Settings may
// request a write through this public boundary but do not own serialization,
// validation, path formation, or atomic replacement policy.
class UserProfileStore final {
public:
    explicit UserProfileStore(QString directory);

    // AGENT-CONTRACT: save validates the complete typed value, proves its
    // strict-loader round trip, and only then atomically replaces
    // `<directory>/<id>.json`. Every failure preserves prior bytes.
    [[nodiscard]] UserProfileStoreResult save(const LayoutProfile &profile) const;

    // AGENT-CONTRACT: remove deletes exactly `<directory>/<id>.json` for a
    // valid id and nothing else. A missing file is NotFound (nothing
    // changed); a failed deletion keeps the file and reports RemoveFailed.
    // Removing a user copy lets an installed profile with the same id show
    // through again (catalog precedence), so Settings uses it to restore a
    // built-in layout and to delete the user's own presets (ADR-0267).
    [[nodiscard]] UserProfileStoreResult remove(const QString &profileId) const;

    [[nodiscard]] const QString &directory() const noexcept { return m_directory; }
    [[nodiscard]] static QString fileNameForId(const QString &profileId);
    [[nodiscard]] static bool isValidProfileId(const QString &profileId);

private:
    QString m_directory;
};

} // namespace QindaQt::Profiles
