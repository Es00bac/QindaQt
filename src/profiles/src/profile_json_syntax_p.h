// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_validation.h"

#include <QByteArray>

namespace QindaQt::Profiles::Internal {

[[nodiscard]] ProfileError validateJsonSyntax(const QByteArray &json,
                                              const QString &origin);

} // namespace QindaQt::Profiles::Internal
