// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gain_loaded_image.h"
#include <QtCore/QMap>
#include <QtCore/QProcessEnvironment>
struct pw_impl_module;

namespace QindaQt::Audio::Test
{
// Private diagnostic receipt is pinned at configure time by its SHA256. It
// records an actual Portage image, not an upstream version-string permission.
// Same-thread read-only checks drive no event loop and never mutate loader env.
// Production borrowed-Core enablement is separate and stays disabled.
class QualifiedGainImage final
{
public:
    [[nodiscard]] bool preflight(QString *reason);
    [[nodiscard]] bool bootstrapStillPinned(bool requireSpa = true) const;
    [[nodiscard]] bool loadedModule(pw_impl_module *) const;
    [[nodiscard]] bool privateChild(qint64 pid) const;
    [[nodiscard]] QMap<QByteArray, QByteArray> bootstrapValues() const;
    [[nodiscard]] bool unpatchedControl() const { return m_unpatched; }
private:
    QMap<QString, GainImageArtifact> m_artifacts;
    QMap<QString, GainImageArtifact> m_evidence;
    QString m_moduleDirectory;
    QString m_spaDirectory;
    QString m_libraryDirectory;
    bool m_ready = false;
    bool m_unpatched = false;
};
} // namespace QindaQt::Audio::Test
