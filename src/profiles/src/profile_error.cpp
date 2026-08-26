// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_validation.h"

#include <QStringList>

namespace QindaQt::Profiles {

QString ProfileError::diagnostic() const
{
    if (!hasError()) {
        return {};
    }

    QStringList locations;
    if (!origin.isEmpty()) {
        locations.append(origin);
    }
    if (!path.isEmpty()) {
        locations.append(path);
    }
    if (byteOffset >= 0) {
        locations.append(QStringLiteral("byte %1").arg(byteOffset));
    }
    if (locations.isEmpty()) {
        return message;
    }
    return QStringLiteral("%1: %2").arg(locations.join(QStringLiteral(": ")), message);
}

} // namespace QindaQt::Profiles
