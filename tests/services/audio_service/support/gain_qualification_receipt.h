// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gain_loaded_image.h"
#include <QtCore/QJsonObject>
#include <QtCore/QMap>

namespace QindaQt::Audio::Test
{
// AGENT-CONTRACT: Python producer/bootstrap and this native gate share private
// receipt schema2, intentionally incompatible with schema1.
// Returns pinned evidence only after the full original/fixed mode and phase
// proof agree. Failed upstream admission needs a separate compiled old-only
// opt-in; it never grants production gain, release or installed permission.
[[nodiscard]] bool gainQualificationReceipt(const QJsonObject &, bool allowFailedOld,
    QMap<QString, GainImageArtifact> *evidence);
} // namespace QindaQt::Audio::Test
