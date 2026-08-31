// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/themes/theme_spec.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Services::Portal {

// Loads at most sixteen absolute directories and 128 bounded theme documents.
// Earlier directories win duplicate ids. Any malformed discovered document
// fails the complete catalog closed; missing directories are ignored.
[[nodiscard]] std::optional<QVector<Themes::ThemeSpec>>
loadPortalAppearanceThemes(const QStringList &directories,
                           QString *error = nullptr);

} // namespace QindaQt::Services::Portal
