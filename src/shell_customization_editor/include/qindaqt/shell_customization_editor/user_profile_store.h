// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/user_profile_store.h"

#include <QString>

namespace QindaQt::ShellCustomizationEditor {

using ProfileStoreErrorCode = Profiles::UserProfileStoreErrorCode;
using ProfileStoreResult = Profiles::UserProfileStoreResult;

// Editor-facing adapter over the profiles module's sole persistence authority.
// This type owns no filesystem or serialization policy; keeping it narrow lets
// the editor inject a destination without reversing the D4 module boundary.
class UserProfileStore final {
public:
    explicit UserProfileStore(QString directory);

    // Serializes and atomically replaces `<directory>/<id>.json`.
    [[nodiscard]] ProfileStoreResult save(const Profiles::LayoutProfile &profile) const;

    [[nodiscard]] const QString &directory() const noexcept { return m_store.directory(); }

    [[nodiscard]] static QString fileNameForId(const QString &profileId);
    // Only plain file-name-safe identifiers are accepted: the id becomes a
    // file name and must never traverse directories.
    [[nodiscard]] static bool isValidProfileId(const QString &profileId);

private:
    Profiles::UserProfileStore m_store;
};

} // namespace QindaQt::ShellCustomizationEditor
