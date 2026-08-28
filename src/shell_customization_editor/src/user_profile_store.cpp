// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <utility>

namespace QindaQt::ShellCustomizationEditor {

UserProfileStore::UserProfileStore(QString directory)
    : m_store(std::move(directory))
{
}

QString UserProfileStore::fileNameForId(const QString &profileId)
{
    return Profiles::UserProfileStore::fileNameForId(profileId);
}

bool UserProfileStore::isValidProfileId(const QString &profileId)
{
    return Profiles::UserProfileStore::isValidProfileId(profileId);
}

ProfileStoreResult UserProfileStore::save(const Profiles::LayoutProfile &profile) const
{
    return m_store.save(profile);
}

} // namespace QindaQt::ShellCustomizationEditor
