// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QByteArray>
#include <QtCore/QString>

struct pw_impl_module;

namespace QindaQt::Audio::Test
{
struct GainImageArtifact {
    QString path;
    QByteArray sha256;
    qint64 bytes = 0;
};
// Test-only, bounded regular-file observation. This is not writer exclusion.
[[nodiscard]] bool exactGainImageFile(const GainImageArtifact &artifact);
[[nodiscard]] bool actualGainCoreImage(const GainImageArtifact &artifact);
[[nodiscard]] bool actualGainModuleImage(pw_impl_module *, const GainImageArtifact &artifact);
[[nodiscard]] bool gainProcessMapsImage(qint64 privateChildPid, const GainImageArtifact &artifact);
} // namespace QindaQt::Audio::Test
