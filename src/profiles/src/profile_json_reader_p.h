// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/profiles/profile_validation.h"

#include <QJsonObject>

namespace QindaQt::Profiles::Internal {

struct ProfileJsonReadResult final {
    LayoutProfile profile;
    ProfileError error;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return !error.hasError();
    }
};

[[nodiscard]] ProfileJsonReadResult readProfileObject(const QJsonObject &root,
                                                      const QString &origin);

} // namespace QindaQt::Profiles::Internal
